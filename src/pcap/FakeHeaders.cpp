#include "FakeHeaders.h"
#include "PcapFormat.h"

namespace pcap {

namespace {
constexpr size_t kEthOff = 0;
constexpr size_t kIpOff = kEthernetHeaderSize;          // 14
constexpr size_t kUdpOff = kIpOff + kIPv4HeaderSize;    // 34
constexpr size_t kPayloadOff = kUdpOff + kUdpHeaderSize; // 42

constexpr uint8_t kDstMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
constexpr uint8_t kSrcMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
constexpr uint16_t kEtherTypeIPv4 = 0x0800;
constexpr uint8_t kIpProtoUdp = 17;
} // namespace

uint16_t internetChecksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    size_t i = 0;
    for (; i + 1 < len; i += 2) {
        sum += getBE16(data + i);
    }
    if (i < len) {
        // Odd trailing byte: treat as the high byte of a zero-padded word.
        sum += static_cast<uint16_t>(data[i]) << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum & 0xFFFF);
}

size_t buildSyntheticFrame(uint8_t* outBuf, size_t outBufCap,
                            const FrameParams& params,
                            const uint8_t* payload, size_t payloadLen) {
    if (payloadLen > 65535 - kIPv4HeaderSize - kUdpHeaderSize) {
        return 0; // would overflow 16-bit IPv4 Total Length / UDP Length
    }
    const size_t total = syntheticFrameSize(payloadLen);
    if (outBufCap < total) {
        return 0;
    }

    // Ethernet II (14 bytes)
    uint8_t* eth = outBuf + kEthOff;
    for (int i = 0; i < 6; ++i) eth[i] = kDstMac[i];
    for (int i = 0; i < 6; ++i) eth[6 + i] = kSrcMac[i];
    putBE16(eth + 12, kEtherTypeIPv4);

    // IPv4 (20 bytes, no options)
    uint8_t* ip = outBuf + kIpOff;
    ip[0] = 0x45; // version 4, IHL 5
    ip[1] = 0x00; // DSCP/ECN
    putBE16(ip + 2, static_cast<uint16_t>(kIPv4HeaderSize + kUdpHeaderSize + payloadLen)); // Total Length
    putBE16(ip + 4, 0x0000); // Identification (cosmetic only, never fragmented)
    putBE16(ip + 6, 0x4000); // Flags=DF, Fragment Offset=0
    ip[8] = params.ttl;
    ip[9] = kIpProtoUdp;
    putBE16(ip + 10, 0x0000); // checksum placeholder, filled below
    putBE32(ip + 12, params.srcIp);
    putBE32(ip + 16, params.dstIp);
    const uint16_t ipChecksum = internetChecksum(ip, kIPv4HeaderSize);
    putBE16(ip + 10, ipChecksum);

    // UDP (8 bytes)
    uint8_t* udp = outBuf + kUdpOff;
    putBE16(udp + 0, params.srcPort);
    putBE16(udp + 2, params.dstPort);
    putBE16(udp + 4, static_cast<uint16_t>(kUdpHeaderSize + payloadLen));
    putBE16(udp + 6, 0x0000); // checksum: RFC 768 "not computed" sentinel, legal for UDP/IPv4

    // Payload
    if (payloadLen > 0) {
        uint8_t* dst = outBuf + kPayloadOff;
        for (size_t i = 0; i < payloadLen; ++i) dst[i] = payload[i];
    }

    return total;
}

bool parseSyntheticFrame(const uint8_t* frame, size_t frameLen, ParsedFrame& out) {
    if (frameLen < kPayloadOff) {
        return false;
    }
    const uint8_t* eth = frame + kEthOff;
    if (getBE16(eth + 12) != kEtherTypeIPv4) {
        return false;
    }
    const uint8_t* ip = frame + kIpOff;
    const uint8_t versionIhl = ip[0];
    if ((versionIhl >> 4) != 4 || (versionIhl & 0x0F) != 5) {
        return false; // only IPv4, no-options frames (IHL==5) are supported
    }
    if (ip[9] != kIpProtoUdp) {
        return false;
    }
    const uint8_t* udp = frame + kUdpOff;
    const uint16_t udpLen = getBE16(udp + 4);
    if (udpLen < kUdpHeaderSize) {
        return false;
    }
    const size_t declaredPayloadLen = udpLen - kUdpHeaderSize;
    if (kPayloadOff + declaredPayloadLen > frameLen) {
        return false; // truncated frame
    }

    out.srcIp = getBE32(ip + 12);
    out.dstIp = getBE32(ip + 16);
    out.srcPort = getBE16(udp + 0);
    out.dstPort = getBE16(udp + 2);
    out.payload = frame + kPayloadOff;
    out.payloadLen = declaredPayloadLen;
    return true;
}

} // namespace pcap
