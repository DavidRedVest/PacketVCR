#include "NetworkInterfaceInfo.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <vector>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace net {

#ifdef _WIN32

std::vector<InterfaceInfo> listInterfaces() {
    std::vector<InterfaceInfo> result;

    ULONG bufLen = 15000; // MSDN-recommended starting size
    std::vector<uint8_t> buf(bufLen);
    IP_ADAPTER_ADDRESSES* addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());

    ULONG ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf.resize(bufLen);
        addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &bufLen);
    }
    if (ret != NO_ERROR) {
        return result;
    }

    for (IP_ADAPTER_ADDRESSES* adapter = addresses; adapter != nullptr; adapter = adapter->Next) {
        const bool isUp = adapter->OperStatus == IfOperStatusUp;
        const bool supportsMulticast = (adapter->Flags & IP_ADAPTER_NO_MULTICAST) == 0;

        char nameBuf[256] = {0};
        int len = WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
        const std::string name = len > 0 ? std::string(nameBuf) : "adapter";

        for (IP_ADAPTER_UNICAST_ADDRESS* ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }
            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            InterfaceInfo info;
            info.name = name;
            info.ipv4Address = ntohl(sin->sin_addr.s_addr);
            info.isUp = isUp;
            info.supportsMulticast = supportsMulticast;
            result.push_back(info);
        }
    }

    return result;
}

#else

std::vector<InterfaceInfo> listInterfaces() {
    std::vector<InterfaceInfo> result;

    ifaddrs* addrs = nullptr;
    if (getifaddrs(&addrs) != 0) {
        return result;
    }

    for (ifaddrs* ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        auto* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);

        InterfaceInfo info;
        info.name = ifa->ifa_name ? ifa->ifa_name : "";
        info.ipv4Address = ntohl(sin->sin_addr.s_addr);
        info.isUp = (ifa->ifa_flags & IFF_UP) != 0;
        info.supportsMulticast = (ifa->ifa_flags & IFF_MULTICAST) != 0;
        result.push_back(info);
    }

    freeifaddrs(addrs);
    return result;
}

#endif

} // namespace net
