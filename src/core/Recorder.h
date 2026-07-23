#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

// Qt-free by design (see project memory: QUdpSocket caused packet loss in a
// prior project). GUI and CLI both drive this the same way: construct,
// setLogCallback, start(), poll stats()/isRunning(), stop().
namespace core {

struct RecorderConfig {
    uint32_t bindOrGroupIp = 0;    // host-order IPv4; multicast range (224.0.0.0/4) triggers an IGMP join
    uint16_t port = 0;
    uint32_t localInterfaceIp = 0; // which NIC to join/bind on; 0 == let the OS pick
    std::string outputPath;        // .pcap file to write
};

struct RecorderStats {
    uint64_t packetCount = 0;
    uint64_t byteCount = 0;
};

class Recorder {
public:
    using LogCallback = std::function<void(const std::string&)>;

    Recorder() = default;
    ~Recorder();

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    // May be called from any thread before start(); the callback itself
    // will be invoked from the internal worker thread once running, so the
    // receiver (e.g. a GUI log console) must marshal to its own thread if
    // needed.
    void setLogCallback(LogCallback cb);

    // Synchronously validates that outputPath can be opened for writing,
    // then spawns the receive worker thread and returns true. Socket setup
    // (bind/multicast join) happens on that thread; if it fails, the
    // failure is reported via the log callback and the thread exits
    // (isRunning() will observably flip back to false shortly after).
    bool start(const RecorderConfig& config);

    // Signals the worker thread to stop and joins it. Safe to call even if
    // not currently running.
    void stop();

    bool isRunning() const;

    // Cheap, lock-free snapshot suitable for polling from a GUI timer.
    RecorderStats stats() const;

private:
    void runLoop(RecorderConfig config);
    void log(const std::string& message);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<uint64_t> packetCount_{0};
    std::atomic<uint64_t> byteCount_{0};
    LogCallback logCallback_;
};

} // namespace core
