#pragma once

#include <cstdint>
#include <cstddef>

// Classic libpcap file format constants (not pcapng).
// Reference: https://wiki.wireshark.org/Development/LibpcapFileFormat
namespace pcap {

constexpr uint32_t kMagicMicros = 0xa1b2c3d4; // native-endian, microsecond-resolution timestamps
constexpr uint16_t kVersionMajor = 2;
constexpr uint16_t kVersionMinor = 4;
constexpr uint32_t kDefaultSnapLen = 65535;
constexpr uint32_t kLinkTypeEthernet = 1; // LINKTYPE_ETHERNET

constexpr size_t kGlobalHeaderSize = 24;
constexpr size_t kRecordHeaderSize = 16;

constexpr size_t kEthernetHeaderSize = 14;
constexpr size_t kIPv4HeaderSize = 20;
constexpr size_t kUdpHeaderSize = 8;
constexpr size_t kSyntheticHeaderSize = kEthernetHeaderSize + kIPv4HeaderSize + kUdpHeaderSize; // 42

struct GlobalHeader {
    uint32_t magicNumber = kMagicMicros;
    uint16_t versionMajor = kVersionMajor;
    uint16_t versionMinor = kVersionMinor;
    int32_t thisZone = 0;
    uint32_t sigFigs = 0;
    uint32_t snapLen = kDefaultSnapLen;
    uint32_t network = kLinkTypeEthernet;
};

struct RecordHeader {
    uint32_t tsSec = 0;
    uint32_t tsUsec = 0;
    uint32_t inclLen = 0; // bytes actually saved (== origLen, we never truncate)
    uint32_t origLen = 0; // 14 + 20 + 8 + payloadLen
};

// Little-endian byte helpers. Written explicitly (never memcpy'd structs)
// to avoid struct padding/alignment hazards across compilers/platforms.
inline void putLE16(uint8_t* dst, uint16_t v) {
    dst[0] = static_cast<uint8_t>(v & 0xFF);
    dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

inline void putLE32(uint8_t* dst, uint32_t v) {
    dst[0] = static_cast<uint8_t>(v & 0xFF);
    dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    dst[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    dst[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

inline uint16_t getLE16(const uint8_t* src) {
    return static_cast<uint16_t>(src[0]) | (static_cast<uint16_t>(src[1]) << 8);
}

inline uint32_t getLE32(const uint8_t* src) {
    return static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) | (static_cast<uint32_t>(src[3]) << 24);
}

inline void putBE16(uint8_t* dst, uint16_t v) {
    dst[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    dst[1] = static_cast<uint8_t>(v & 0xFF);
}

inline void putBE32(uint8_t* dst, uint32_t v) {
    dst[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    dst[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    dst[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    dst[3] = static_cast<uint8_t>(v & 0xFF);
}

inline uint16_t getBE16(const uint8_t* src) {
    return (static_cast<uint16_t>(src[0]) << 8) | static_cast<uint16_t>(src[1]);
}

inline uint32_t getBE32(const uint8_t* src) {
    return (static_cast<uint32_t>(src[0]) << 24) | (static_cast<uint32_t>(src[1]) << 16) |
           (static_cast<uint32_t>(src[2]) << 8) | static_cast<uint32_t>(src[3]);
}

} // namespace pcap
