#pragma once

#include "FakeHeaders.h"
#include "PcapFormat.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace pcap {

// Streaming reader for classic pcap files. Exposes two layers:
//  - next() / record-level: raw synthesized frame bytes + capture timestamp
//  - nextPacket(): the same, already stripped down to {srcIp, dstIp,
//    srcPort, dstPort, payload} via FakeHeaders::parseSyntheticFrame
//
// Streaming (rather than loading the whole file up front) keeps this class
// usable for arbitrarily large captures; callers that want random access
// (e.g. Player) can drain nextPacket() into their own vector.
class PcapReader {
public:
    struct Record {
        std::chrono::system_clock::time_point timestamp;
        std::vector<uint8_t> frame; // synthesized Ethernet+IPv4+UDP+payload
    };

    struct Packet {
        std::chrono::system_clock::time_point timestamp;
        uint32_t srcIp = 0;
        uint32_t dstIp = 0;
        uint16_t srcPort = 0;
        uint16_t dstPort = 0;
        std::vector<uint8_t> payload; // owned copy (frame buffer is transient)
    };

    PcapReader() = default;

    PcapReader(const PcapReader&) = delete;
    PcapReader& operator=(const PcapReader&) = delete;

    // Opens `path` and validates the 24-byte global header (magic number,
    // link type). Returns false if the file can't be opened or doesn't look
    // like a pcap file this reader understands (only LINKTYPE_ETHERNET,
    // native-endian magic, is supported -- our own writer always produces
    // this; anything else is a clear, reported error rather than silently
    // misparsed).
    bool open(const std::string& path);

    bool isOpen() const { return file_.is_open(); }

    // Empty when the last failed read was a clean EOF. Otherwise describes
    // why open()/next()/nextPacket() failed.
    const std::string& lastError() const { return lastError_; }
    bool hasError() const { return !lastError_.empty(); }

    // Reads the next raw record. Returns false at EOF or on a truncated/
    // corrupt record (subsequent calls also return false).
    bool next(Record& out);

    // Reads the next record and parses it down to a Packet. Returns false
    // at EOF, on a truncated record, or if the record doesn't parse as a
    // synthesized Ethernet+IPv4+UDP frame.
    bool nextPacket(Packet& out);

    void close();

private:
    std::ifstream file_;
    bool swapped_ = false;        // true if the file was written on a big-endian host (rare; we target LE only)
    bool nanoResolution_ = false; // true if the file uses nanosecond (vs. microsecond) timestamps
    bool eofOrError_ = false;
    std::string lastError_;
};

} // namespace pcap
