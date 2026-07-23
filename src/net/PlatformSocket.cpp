#include "PlatformSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace net {

#ifdef _WIN32
namespace {
// Constructed once (thread-safe magic statics) the first time a
// PlatformSocket is created; WSACleanup() runs at process exit via the
// destructor. Avoids requiring callers to manage Winsock lifetime.
struct WinsockInit {
    WinsockInit() {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WinsockInit() { WSACleanup(); }
};
void ensureWinsockInit() {
    static WinsockInit init;
}
} // namespace
#endif

PlatformSocket::PlatformSocket() {
#ifdef _WIN32
    ensureWinsockInit();
    sock_ = static_cast<uintptr_t>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
#else
    sock_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
}

PlatformSocket::~PlatformSocket() {
    close();
}

bool PlatformSocket::isValid() const {
#ifdef _WIN32
    return static_cast<SOCKET>(sock_) != INVALID_SOCKET;
#else
    return sock_ >= 0;
#endif
}

void PlatformSocket::close() {
    if (!isValid()) {
        return;
    }
#ifdef _WIN32
    ::closesocket(static_cast<SOCKET>(sock_));
    sock_ = static_cast<uintptr_t>(INVALID_SOCKET);
#else
    ::close(sock_);
    sock_ = -1;
#endif
}

bool PlatformSocket::bindAny(uint16_t port, bool reuseAddr) {
    if (!isValid()) {
        return false;
    }

    if (reuseAddr) {
        const int opt = 1;
#ifdef _WIN32
        ::setsockopt(static_cast<SOCKET>(sock_), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
        // macOS/BSD: needed in addition to SO_REUSEADDR for multiple
        // listeners to share the same multicast port (e.g. this tool
        // running alongside Wireshark).
        ::setsockopt(sock_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
#endif
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

#ifdef _WIN32
    return ::bind(static_cast<SOCKET>(sock_), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
#else
    return ::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
#endif
}

bool PlatformSocket::joinMulticastGroup(uint32_t groupIp, uint32_t localInterfaceIp) {
    if (!isValid()) {
        return false;
    }
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = htonl(groupIp);
    mreq.imr_interface.s_addr = htonl(localInterfaceIp);
#ifdef _WIN32
    return ::setsockopt(static_cast<SOCKET>(sock_), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == 0;
#else
    return ::setsockopt(sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == 0;
#endif
}

bool PlatformSocket::leaveMulticastGroup(uint32_t groupIp, uint32_t localInterfaceIp) {
    if (!isValid()) {
        return false;
    }
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = htonl(groupIp);
    mreq.imr_interface.s_addr = htonl(localInterfaceIp);
#ifdef _WIN32
    return ::setsockopt(static_cast<SOCKET>(sock_), IPPROTO_IP, IP_DROP_MEMBERSHIP,
                         reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == 0;
#else
    return ::setsockopt(sock_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == 0;
#endif
}

bool PlatformSocket::setMulticastTTL(uint8_t ttl) {
    if (!isValid()) {
        return false;
    }
    // IP_MULTICAST_TTL's expected option type differs by platform: BSD/macOS
    // wants a u_char, Linux/Windows want an int.
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    const unsigned char value = ttl;
    return ::setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_TTL, &value, sizeof(value)) == 0;
#elif defined(_WIN32)
    const int value = ttl;
    return ::setsockopt(static_cast<SOCKET>(sock_), IPPROTO_IP, IP_MULTICAST_TTL,
                         reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
#else
    const int value = ttl;
    return ::setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_TTL, &value, sizeof(value)) == 0;
#endif
}

bool PlatformSocket::setMulticastSendInterface(uint32_t localInterfaceIp) {
    if (!isValid()) {
        return false;
    }
    in_addr addr{};
    addr.s_addr = htonl(localInterfaceIp);
#ifdef _WIN32
    return ::setsockopt(static_cast<SOCKET>(sock_), IPPROTO_IP, IP_MULTICAST_IF,
                         reinterpret_cast<const char*>(&addr), sizeof(addr)) == 0;
#else
    return ::setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) == 0;
#endif
}

bool PlatformSocket::setRecvBufferSize(int bytes) {
    if (!isValid()) {
        return false;
    }
#ifdef _WIN32
    return ::setsockopt(static_cast<SOCKET>(sock_), SOL_SOCKET, SO_RCVBUF,
                         reinterpret_cast<const char*>(&bytes), sizeof(bytes)) == 0;
#else
    return ::setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof(bytes)) == 0;
#endif
}

bool PlatformSocket::setSendBufferSize(int bytes) {
    if (!isValid()) {
        return false;
    }
#ifdef _WIN32
    return ::setsockopt(static_cast<SOCKET>(sock_), SOL_SOCKET, SO_SNDBUF,
                         reinterpret_cast<const char*>(&bytes), sizeof(bytes)) == 0;
#else
    return ::setsockopt(sock_, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes)) == 0;
#endif
}

RecvResult PlatformSocket::recvFrom(uint8_t* buf, size_t cap, uint32_t timeoutMs,
                                     size_t& outLen, uint32_t& outSrcIp, uint16_t& outSrcPort) {
    if (!isValid()) {
        return RecvResult::Error;
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    int nfds = 0;
#ifdef _WIN32
    FD_SET(static_cast<SOCKET>(sock_), &readSet);
#else
    FD_SET(sock_, &readSet);
    nfds = sock_ + 1;
#endif

    timeval tv;
    tv.tv_sec = static_cast<long>(timeoutMs / 1000);
    tv.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);

    const int rv = ::select(nfds, &readSet, nullptr, nullptr, &tv);
    if (rv == 0) {
        return RecvResult::Timeout;
    }
    if (rv < 0) {
#ifndef _WIN32
        if (errno == EINTR) {
            return RecvResult::Timeout;
        }
#endif
        return RecvResult::Error;
    }

    sockaddr_in srcAddr{};
    socklen_t srcLen = sizeof(srcAddr);
#ifdef _WIN32
    const int n = ::recvfrom(static_cast<SOCKET>(sock_), reinterpret_cast<char*>(buf), static_cast<int>(cap), 0,
                              reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
#else
    const ssize_t n = ::recvfrom(sock_, buf, cap, 0, reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
#endif
    if (n < 0) {
        return RecvResult::Error;
    }

    outLen = static_cast<size_t>(n);
    outSrcIp = ntohl(srcAddr.sin_addr.s_addr);
    outSrcPort = ntohs(srcAddr.sin_port);
    return RecvResult::Data;
}

bool PlatformSocket::sendTo(const uint8_t* buf, size_t len, uint32_t dstIp, uint16_t dstPort) {
    if (!isValid()) {
        return false;
    }
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(dstIp);
    dst.sin_port = htons(dstPort);

#ifdef _WIN32
    const int n = ::sendto(static_cast<SOCKET>(sock_), reinterpret_cast<const char*>(buf), static_cast<int>(len), 0,
                            reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
#else
    const ssize_t n = ::sendto(sock_, buf, len, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
#endif
    return n >= 0 && static_cast<size_t>(n) == len;
}

std::string PlatformSocket::lastErrorMessage() {
#ifdef _WIN32
    return "WSA error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

} // namespace net
