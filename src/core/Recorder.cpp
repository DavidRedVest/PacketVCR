#include "Recorder.h"

#include "PcapWriter.h"
#include "PlatformSocket.h"

#include <chrono>
#include <vector>

namespace core {

namespace {
bool isMulticast(uint32_t ip) {
    const uint8_t topOctet = static_cast<uint8_t>(ip >> 24);
    return topOctet >= 224 && topOctet <= 239;
}

constexpr size_t kMaxDatagramSize = 65535;
constexpr uint32_t kRecvTimeoutMs = 200; // lets the loop notice stopRequested_ promptly without busy-polling
constexpr int kRecvBufferBytes = 1 << 20; // 1MB, generous headroom for bursty high-bitrate video
} // namespace

Recorder::~Recorder() {
    stop();
}

void Recorder::setLogCallback(LogCallback cb) {
    logCallback_ = std::move(cb);
}

void Recorder::log(const std::string& message) {
    if (logCallback_) {
        logCallback_(message);
    }
}

bool Recorder::start(const RecorderConfig& config) {
    if (running_.load()) {
        return false;
    }

    // Synchronous pre-check: fail fast on a bad output path rather than
    // silently spawning a thread that immediately dies.
    {
        pcap::PcapWriter probe;
        if (!probe.open(config.outputPath)) {
            log("cannot open output file for writing: " + config.outputPath);
            return false;
        }
    }

    stopRequested_.store(false);
    packetCount_.store(0);
    byteCount_.store(0);
    running_.store(true);
    // A previous run that exited early on its own (setup failure: bad
    // output path, bind failure, ...) sets running_ = false without
    // anyone joining thread_, so it's still joinable here even though the
    // OS thread has already exited. Reassigning std::thread over a
    // joinable one calls std::terminate(), so reap it first.
    if (thread_.joinable()) {
        thread_.join();
    }
    thread_ = std::thread(&Recorder::runLoop, this, config);
    return true;
}

void Recorder::stop() {
    stopRequested_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool Recorder::isRunning() const {
    return running_.load();
}

RecorderStats Recorder::stats() const {
    RecorderStats s;
    s.packetCount = packetCount_.load(std::memory_order_relaxed);
    s.byteCount = byteCount_.load(std::memory_order_relaxed);
    return s;
}

void Recorder::runLoop(RecorderConfig config) {
    pcap::PcapWriter writer;
    if (!writer.open(config.outputPath)) {
        log("cannot open output file for writing: " + config.outputPath);
        running_.store(false);
        return;
    }

    net::PlatformSocket sock;
    if (!sock.isValid()) {
        log("failed to create socket: " + net::PlatformSocket::lastErrorMessage());
        running_.store(false);
        return;
    }
    sock.setRecvBufferSize(kRecvBufferBytes);

    if (!sock.bindAny(config.port, /*reuseAddr=*/true)) {
        log("failed to bind port " + std::to_string(config.port) + ": " + net::PlatformSocket::lastErrorMessage());
        running_.store(false);
        return;
    }

    const bool multicast = isMulticast(config.bindOrGroupIp);
    if (multicast) {
        if (!sock.joinMulticastGroup(config.bindOrGroupIp, config.localInterfaceIp)) {
            log("failed to join multicast group: " + net::PlatformSocket::lastErrorMessage());
            running_.store(false);
            return;
        }
    }

    log(std::string("recording started (") + (multicast ? "multicast" : "unicast") + ") -> " + config.outputPath);

    std::vector<uint8_t> buf(kMaxDatagramSize);
    auto lastFlush = std::chrono::steady_clock::now();

    while (!stopRequested_.load()) {
        size_t len = 0;
        uint32_t srcIp = 0;
        uint16_t srcPort = 0;
        const auto result = sock.recvFrom(buf.data(), buf.size(), kRecvTimeoutMs, len, srcIp, srcPort);

        if (result == net::RecvResult::Data) {
            const auto captureTime = std::chrono::system_clock::now();
            pcap::FrameParams params;
            params.srcIp = srcIp;
            params.dstIp = config.bindOrGroupIp;
            params.srcPort = srcPort;
            params.dstPort = config.port;
            if (writer.writePacket(captureTime, params, buf.data(), len)) {
                packetCount_.fetch_add(1, std::memory_order_relaxed);
                byteCount_.fetch_add(len, std::memory_order_relaxed);
            } else {
                log("failed to write pcap record (disk full?)");
            }
        } else if (result == net::RecvResult::Error) {
            log("recv failed: " + net::PlatformSocket::lastErrorMessage());
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastFlush > std::chrono::seconds(1)) {
            writer.flush();
            lastFlush = now;
        }
    }

    if (multicast) {
        sock.leaveMulticastGroup(config.bindOrGroupIp, config.localInterfaceIp);
    }
    writer.close();
    log("recording stopped");
    running_.store(false);
}

} // namespace core
