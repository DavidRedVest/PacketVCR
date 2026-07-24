#include "PcapReader.h"

namespace pcap {

namespace {
constexpr uint32_t kMagicMicrosSwapped = 0xd4c3b2a1;
constexpr uint32_t kMagicNano = 0xa1b23c4d;
constexpr uint32_t kMagicNanoSwapped = 0x4d3cb2a1;

// Guards against corrupt/garbage incl_len values causing an unreasonable
// allocation; a synthesized frame can never legitimately exceed 65535
// (IPv4 Total Length is 16-bit) + the 42-byte fake L2/L3/L4 headers.
constexpr uint32_t kMaxSaneRecordLen = 65535 + static_cast<uint32_t>(kSyntheticHeaderSize);
} // namespace

bool PcapReader::open(const std::string& path) {
    close();
    lastError_.clear();
    file_.open(path, std::ios::binary);
    if (!file_.is_open()) {
        lastError_ = "cannot open file";
        return false;
    }

    uint8_t hdr[kGlobalHeaderSize];
    file_.read(reinterpret_cast<char*>(hdr), kGlobalHeaderSize);
    if (!file_ || file_.gcount() != static_cast<std::streamsize>(kGlobalHeaderSize)) {
        lastError_ = "truncated pcap global header";
        file_.close();
        return false;
    }

    const uint32_t magic = getLE32(hdr + 0);
    if (magic == kMagicMicros) {
        swapped_ = false;
        nanoResolution_ = false;
    } else if (magic == kMagicNano) {
        swapped_ = false;
        nanoResolution_ = true;
    } else if (magic == kMagicMicrosSwapped) {
        swapped_ = true;
        nanoResolution_ = false;
    } else if (magic == kMagicNanoSwapped) {
        swapped_ = true;
        nanoResolution_ = true;
    } else {
        lastError_ = "unsupported or invalid pcap magic number";
        file_.close();
        return false;
    }

    const uint32_t network = swapped_ ? getBE32(hdr + 20) : getLE32(hdr + 20);
    if (network != kLinkTypeEthernet) {
        lastError_ = "unsupported pcap link type";
        file_.close();
        return false;
    }

    eofOrError_ = false;
    return true;
}

bool PcapReader::next(Record& out) {
    if (!file_.is_open() || eofOrError_) {
        return false;
    }

    uint8_t recordHeader[kRecordHeaderSize];
    file_.read(reinterpret_cast<char*>(recordHeader), kRecordHeaderSize);
    if (!file_ || file_.gcount() != static_cast<std::streamsize>(kRecordHeaderSize)) {
        const auto got = file_.gcount();
        eofOrError_ = true;
        if (got != 0) {
            lastError_ = "truncated pcap record header";
        }
        return false;
    }

    uint32_t tsSec, tsFrac, inclLen;
    if (swapped_) {
        tsSec = getBE32(recordHeader + 0);
        tsFrac = getBE32(recordHeader + 4);
        inclLen = getBE32(recordHeader + 8);
    } else {
        tsSec = getLE32(recordHeader + 0);
        tsFrac = getLE32(recordHeader + 4);
        inclLen = getLE32(recordHeader + 8);
    }

    if (inclLen > kMaxSaneRecordLen) {
        eofOrError_ = true;
        lastError_ = "pcap record length is unreasonably large";
        return false;
    }

    std::vector<uint8_t> buf(inclLen);
    if (inclLen > 0) {
        file_.read(reinterpret_cast<char*>(buf.data()), inclLen);
        if (!file_ || static_cast<uint32_t>(file_.gcount()) != inclLen) {
            eofOrError_ = true;
            lastError_ = "truncated pcap record payload";
            return false;
        }
    }

    const uint64_t usec = nanoResolution_ ? (tsFrac / 1000) : tsFrac;
    out.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(tsSec)) +
                     std::chrono::microseconds(usec);
    out.frame = std::move(buf);
    return true;
}

bool PcapReader::nextPacket(Packet& out) {
    Record record;
    if (!next(record)) {
        return false;
    }

    ParsedFrame parsed;
    if (!parseSyntheticFrame(record.frame.data(), record.frame.size(), parsed)) {
        eofOrError_ = true;
        lastError_ = "pcap record is not an Ethernet/IPv4/UDP frame PacketVCR can replay";
        return false;
    }

    out.timestamp = record.timestamp;
    out.srcIp = parsed.srcIp;
    out.dstIp = parsed.dstIp;
    out.srcPort = parsed.srcPort;
    out.dstPort = parsed.dstPort;
    out.payload.assign(parsed.payload, parsed.payload + parsed.payloadLen);
    return true;
}

void PcapReader::close() {
    if (file_.is_open()) {
        file_.close();
    }
    eofOrError_ = false;
    lastError_.clear();
}

} // namespace pcap
