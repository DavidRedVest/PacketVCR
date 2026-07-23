#pragma once

#include "FakeHeaders.h"
#include "PcapFormat.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace pcap {

// Writes a classic (non-pcapng) pcap file. Each written packet is a
// synthesized Ethernet+IPv4+UDP frame wrapping the caller's raw UDP
// payload (see FakeHeaders.h).
//
// Not thread-safe: intended to be driven from a single thread (the
// Recorder's receive path).
class PcapWriter {
public:
    PcapWriter() = default;
    ~PcapWriter();

    PcapWriter(const PcapWriter&) = delete;
    PcapWriter& operator=(const PcapWriter&) = delete;

    // Opens `path` for writing and immediately writes the 24-byte global
    // header. Returns false on failure (e.g. can't create the file).
    bool open(const std::string& path, uint32_t snapLen = kDefaultSnapLen);

    bool isOpen() const { return file_.is_open(); }

    // Synthesizes a frame from `params` + payload and appends one pcap
    // record. `captureTime` is a wall-clock timestamp (pcap requires
    // epoch-based timestamps, not a monotonic clock).
    // Returns false if not open, or if the write fails, or if payloadLen
    // would overflow the synthesized frame's 16-bit length fields.
    bool writePacket(std::chrono::system_clock::time_point captureTime,
                      const FrameParams& params,
                      const uint8_t* payload, size_t payloadLen);

    // Flushes the underlying file stream. Call periodically (e.g. from a
    // ~1s timer) rather than after every packet -- per-packet fflush is a
    // measurable bottleneck at realistic video-stream packet rates.
    void flush();

    // Flushes and closes the file. Safe to call multiple times.
    void close();

private:
    std::ofstream file_;
    std::vector<uint8_t> scratch_;
};

} // namespace pcap
