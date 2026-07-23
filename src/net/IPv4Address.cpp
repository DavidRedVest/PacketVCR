#include "IPv4Address.h"

#include <cstdio>

namespace net {

bool parseIPv4(const std::string& text, uint32_t& outIp) {
    unsigned a, b, c, d;
    if (std::sscanf(text.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return false;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }
    outIp = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

std::string formatIPv4(uint32_t ip) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (ip >> 24) & 0xFFu, (ip >> 16) & 0xFFu, (ip >> 8) & 0xFFu, ip & 0xFFu);
    return buf;
}

} // namespace net
