#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Cross-platform network interface enumeration, used to populate the
// GUI's "which NIC to join/send multicast on" picker without depending on
// Qt::Network (QNetworkInterface). POSIX uses getifaddrs(), Windows uses
// GetAdaptersAddresses().
namespace net {

struct InterfaceInfo {
    std::string name;         // OS interface name, e.g. "en0", "eth0"; on Windows, the adapter's friendly name
    uint32_t ipv4Address = 0; // host-order; 0 if the interface has no IPv4 address
    bool isUp = false;
    bool supportsMulticast = false;
};

// Returns one entry per (interface, IPv4 address) pair that has an address
// assigned. Interfaces with no IPv4 address are omitted (nothing useful to
// bind/join on for this tool).
std::vector<InterfaceInfo> listInterfaces();

} // namespace net
