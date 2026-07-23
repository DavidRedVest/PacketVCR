#pragma once

#include <cstdint>
#include <cstddef>

// Synthesizes/parses a minimal Ethernet II + IPv4 + UDP frame around a raw
// UDP payload, purely so recorded packets can be written into a standard
// pcap file and inspected in Wireshark. This module never touches the
// network itself and has no Qt dependency.
//
// Only the Ethernet MAC addresses and the IPv4 TTL are fabricated
// placeholders (application-layer UDP sockets don't expose either). IP
// source/destination addresses and UDP source/destination ports are always
// the real values observed on the socket.
namespace pcap {

// IPv4 addresses are represented as host-order 32-bit integers, i.e. the
// same numeric value as `(a<<24)|(b<<16)|(c<<8)|d` for dotted-quad a.b.c.d.
struct FrameParams {
    uint32_t srcIp = 0;
    uint32_t dstIp = 0;
    uint16_t srcPort = 0;
    uint16_t dstPort = 0;
    uint8_t ttl = 64; // placeholder: real received TTL isn't available via QUdpSocket
};

struct ParsedFrame {
    uint32_t srcIp = 0;
    uint32_t dstIp = 0;
    uint16_t srcPort = 0;
    uint16_t dstPort = 0;
    const uint8_t* payload = nullptr; // points into the frame buffer passed to parseSyntheticFrame
    size_t payloadLen = 0;
};

// RFC 1071 one's-complement checksum over `len` bytes (big-endian 16-bit
// words, zero-padded if `len` is odd). Used for the IPv4 header checksum.
uint16_t internetChecksum(const uint8_t* data, size_t len);

// Total synthesized frame size for a given payload length (42 + payloadLen).
constexpr size_t syntheticFrameSize(size_t payloadLen) {
    return 14 + 20 + 8 + payloadLen;
}

// Writes Ethernet(14) + IPv4(20) + UDP(8) + payload into outBuf.
// outBufCap must be >= syntheticFrameSize(payloadLen).
// Returns the number of bytes written (== syntheticFrameSize(payloadLen)),
// or 0 if outBufCap is too small or payloadLen would overflow the 16-bit
// IPv4 Total Length field (payloadLen > 65535 - 28).
size_t buildSyntheticFrame(uint8_t* outBuf, size_t outBufCap,
                            const FrameParams& params,
                            const uint8_t* payload, size_t payloadLen);

// Parses a frame previously produced by buildSyntheticFrame (or any
// well-formed Ethernet II/IPv4/UDP frame with IHL==5). `out.payload` points
// into `frame`, valid for as long as `frame` is. Returns false if the frame
// is too short or doesn't look like Ethernet+IPv4(no options)+UDP.
bool parseSyntheticFrame(const uint8_t* frame, size_t frameLen, ParsedFrame& out);

} // namespace pcap
