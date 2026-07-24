#include "Player.h"

#include "PcapReader.h"
#include "PlatformSocket.h"

#include <algorithm>

namespace core {

namespace {
bool isMulticast(uint32_t ip) {
    const uint8_t topOctet = static_cast<uint8_t>(ip >> 24);
    return topOctet >= 224 && topOctet <= 239;
}
} // namespace

Player::~Player() {
    stop();
}

void Player::setLogCallback(LogCallback cb) {
    logCallback_ = std::move(cb);
}

void Player::log(const std::string& message) {
    if (logCallback_) {
        logCallback_(message);
    }
}

bool Player::start(const PlayerConfig& config) {
    if (running_.load()) {
        return false;
    }

    pcap::PcapReader reader;
    if (!reader.open(config.inputPath)) {
        log("cannot open input file: " + config.inputPath);
        return false;
    }

    std::vector<PlaybackPacket> loaded;
    pcap::PcapReader::Packet pkt;
    bool haveFirstTs = false;
    std::chrono::system_clock::time_point firstTs;
    while (reader.nextPacket(pkt)) {
        if (!haveFirstTs) {
            firstTs = pkt.timestamp;
            haveFirstTs = true;
        }
        PlaybackPacket pp;
        pp.offsetFromStart = std::chrono::duration_cast<std::chrono::microseconds>(pkt.timestamp - firstTs);
        pp.payload = std::move(pkt.payload);
        loaded.push_back(std::move(pp));
    }
    reader.close();

    if (loaded.empty()) {
        log("input file has no packets: " + config.inputPath);
        return false;
    }

    packets_ = std::move(loaded);
    stopRequested_.store(false);
    paused_.store(false);
    accumulatedPauseUs_.store(0);
    packetsSent_.store(0);
    bytesSent_.store(0);
    currentIndex_.store(0);
    running_.store(true);
    // A previous run that finished on its own (playback reached the end,
    // no loop) sets running_ = false without anyone joining thread_, so
    // it's still joinable here even though the OS thread has already
    // exited. Reassigning std::thread over a joinable one calls
    // std::terminate(), so reap it first.
    if (thread_.joinable()) {
        thread_.join();
    }
    thread_ = std::thread(&Player::runLoop, this, config);
    return true;
}

void Player::stop() {
    stopRequested_.store(true);
    // In case playback is currently paused, unpause so the worker thread
    // notices stopRequested_ promptly instead of sleeping indefinitely
    // in the paused-wait branch.
    paused_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Player::pause() {
    std::lock_guard<std::mutex> lock(pauseMutex_);
    if (!paused_.exchange(true)) {
        pauseStart_ = std::chrono::steady_clock::now();
    }
}

void Player::resume() {
    std::lock_guard<std::mutex> lock(pauseMutex_);
    if (paused_.exchange(false)) {
        const auto elapsed = std::chrono::steady_clock::now() - pauseStart_;
        accumulatedPauseUs_.fetch_add(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
    }
}

bool Player::isPaused() const {
    return paused_.load();
}

bool Player::isRunning() const {
    return running_.load();
}

PlayerStats Player::stats() const {
    PlayerStats s;
    s.packetsSent = packetsSent_.load(std::memory_order_relaxed);
    s.bytesSent = bytesSent_.load(std::memory_order_relaxed);
    s.totalPackets = packets_.size();
    s.currentIndex = currentIndex_.load(std::memory_order_relaxed);
    return s;
}

void Player::runLoop(PlayerConfig config) {
    net::PlatformSocket sock;
    if (!sock.isValid()) {
        log("failed to create socket: " + net::PlatformSocket::lastErrorMessage());
        running_.store(false);
        return;
    }
    sock.setSendBufferSize(1 << 20);

    const bool multicast = isMulticast(config.destIp);
    if (multicast) {
        sock.setMulticastTTL(config.multicastTtl);
        if (config.localInterfaceIp != 0) {
            sock.setMulticastSendInterface(config.localInterfaceIp);
        }
    }

    const double speed = config.speedMultiplier > 0.0 ? config.speedMultiplier : 1.0;

    log("playback started -> " + std::to_string(packets_.size()) + " packets");

    while (!stopRequested_.load()) {
        // Anchored absolute-deadline scheduling: every packet's deadline is
        // computed from this fixed anchor (plus accumulated pause time),
        // never from the previous packet's actual send time. Overshoot on
        // one packet therefore does not compound into the next.
        const auto anchor = std::chrono::steady_clock::now();
        accumulatedPauseUs_.store(0);

        for (size_t i = 0; i < packets_.size(); ++i) {
            if (stopRequested_.load()) {
                break;
            }

            const auto& pkt = packets_[i];
            const int64_t scaledOffsetUs = static_cast<int64_t>(static_cast<double>(pkt.offsetFromStart.count()) / speed);

            while (true) {
                if (stopRequested_.load()) {
                    break;
                }
                if (paused_.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
                const auto deadline = anchor + std::chrono::microseconds(scaledOffsetUs) +
                                       std::chrono::microseconds(accumulatedPauseUs_.load());
                const auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    break;
                }
                const auto remaining = deadline - now;
                if (remaining > std::chrono::milliseconds(20)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                } else {
                    std::this_thread::sleep_for(remaining);
                }
            }
            if (stopRequested_.load()) {
                break;
            }

            if (!sock.sendTo(pkt.payload.data(), pkt.payload.size(), config.destIp, config.destPort)) {
                log("send failed (buffer full?): " + net::PlatformSocket::lastErrorMessage());
            } else {
                packetsSent_.fetch_add(1, std::memory_order_relaxed);
                bytesSent_.fetch_add(pkt.payload.size(), std::memory_order_relaxed);
            }
            currentIndex_.store(i + 1, std::memory_order_relaxed);
        }

        if (!config.loop || stopRequested_.load()) {
            break;
        }
        currentIndex_.store(0, std::memory_order_relaxed);
    }

    log("playback stopped");
    running_.store(false);
}

} // namespace core
