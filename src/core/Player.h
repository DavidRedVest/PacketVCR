#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Qt-free by design, same rationale as Recorder (see project memory on
// avoiding QUdpSocket for network I/O).
namespace core {

struct PlayerConfig {
    std::string inputPath;         // .pcap file to replay
    uint32_t destIp = 0;           // host-order IPv4, unicast or multicast
    uint16_t destPort = 0;
    uint32_t localInterfaceIp = 0; // egress NIC for multicast send; 0 == let the OS pick
    uint8_t multicastTtl = 1;      // only applied when destIp is multicast
    double speedMultiplier = 1.0;  // 1.0 == original pacing; 2.0 == twice as fast
    bool loop = false;
};

struct PlayerStats {
    uint64_t packetsSent = 0;
    uint64_t bytesSent = 0;
    uint64_t totalPackets = 0; // for a GUI progress bar
    uint64_t currentIndex = 0;
};

class Player {
public:
    using LogCallback = std::function<void(const std::string&)>;

    Player() = default;
    ~Player();

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    void setLogCallback(LogCallback cb);

    // Synchronously loads and parses the whole pcap file (so start() can
    // fail fast on a bad/missing file), computing each packet's offset
    // relative to the first packet's timestamp, then spawns the playback
    // worker thread which sends packets using absolute-deadline scheduling
    // anchored to a fixed start point (bounded per-packet jitter, no
    // cumulative drift over a long file -- see Player.cpp).
    bool start(const PlayerConfig& config);

    void stop();

    // Pausing freezes playback in place; resuming continues from exactly
    // where it left off (all subsequent deadlines shift out by the pause
    // duration, relative timing between packets is unaffected).
    void pause();
    void resume();
    bool isPaused() const;

    bool isRunning() const;

    PlayerStats stats() const;

private:
    struct PlaybackPacket {
        std::chrono::microseconds offsetFromStart;
        std::vector<uint8_t> payload;
    };

    void runLoop(PlayerConfig config);
    void log(const std::string& message);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> paused_{false};
    std::atomic<int64_t> accumulatedPauseUs_{0};
    std::atomic<uint64_t> packetsSent_{0};
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> currentIndex_{0};

    std::mutex pauseMutex_;
    std::chrono::steady_clock::time_point pauseStart_;

    std::vector<PlaybackPacket> packets_;
    LogCallback logCallback_;
};

} // namespace core
