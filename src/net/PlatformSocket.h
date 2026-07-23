#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Thin cross-platform UDP socket wrapper: POSIX BSD sockets on Linux/macOS,
// Winsock2 on Windows. Deliberately has zero Qt dependency -- see project
// memory: QUdpSocket caused packet loss/latency in a prior project, so all
// network I/O here goes through the OS socket API directly.
//
// IPv4 addresses are represented as host-order uint32_t everywhere in this
// module (same convention as pcap::FrameParams), e.g. 224.1.1.4 ==
// 0xE0010104.
namespace net {

enum class RecvResult { Data, Timeout, Error };

class PlatformSocket {
public:
    PlatformSocket();
    ~PlatformSocket();

    PlatformSocket(const PlatformSocket&) = delete;
    PlatformSocket& operator=(const PlatformSocket&) = delete;

    // Creates the underlying UDP socket and binds it to INADDR_ANY:port.
    // Always bind-to-any then join explicitly for multicast (see
    // joinMulticastGroup) -- binding directly to a multicast address is
    // inconsistent across platforms, especially Windows.
    bool bindAny(uint16_t port, bool reuseAddr = true);

    // groupIp must be a multicast address (224.0.0.0/4). localInterfaceIp
    // selects which NIC to join on (0 == let the OS pick the default).
    bool joinMulticastGroup(uint32_t groupIp, uint32_t localInterfaceIp = 0);
    bool leaveMulticastGroup(uint32_t groupIp, uint32_t localInterfaceIp = 0);

    // Only meaningful when sending to a multicast destination.
    bool setMulticastTTL(uint8_t ttl);
    bool setMulticastSendInterface(uint32_t localInterfaceIp);

    bool setRecvBufferSize(int bytes);
    bool setSendBufferSize(int bytes);

    // Waits up to timeoutMs for a datagram (short timeouts, e.g. 200ms, let
    // callers poll a stop flag without indefinite blocking). On
    // RecvResult::Data, buf/outLen/outSrcIp/outSrcPort are filled in.
    RecvResult recvFrom(uint8_t* buf, size_t cap, uint32_t timeoutMs,
                         size_t& outLen, uint32_t& outSrcIp, uint16_t& outSrcPort);

    // Returns false (and logs nothing itself -- caller decides how to
    // surface send failures, e.g. rate-limited logging) on a socket error
    // such as a full send buffer (ENOBUFS/WSAENOBUFS have no automatic
    // retry under UDP).
    bool sendTo(const uint8_t* buf, size_t len, uint32_t dstIp, uint16_t dstPort);

    void close();
    bool isValid() const;

    // Human-readable OS error message for the last failed call on this
    // thread (errno / WSAGetLastError), useful for log callbacks.
    static std::string lastErrorMessage();

private:
#ifdef _WIN32
    uintptr_t sock_; // actually a Windows SOCKET; kept opaque to avoid pulling winsock2.h into this header
#else
    int sock_;
#endif
};

} // namespace net
