#include "IPv4Address.h"

#include <cerrno>
#include <cstdlib>
#include <cstdio>

namespace net {

bool parseIPv4(const std::string& text, uint32_t& outIp) {
    uint32_t parts[4] = {0, 0, 0, 0};
    const char* cursor = text.c_str();

    for (int i = 0; i < 4; ++i) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }

        char* end = nullptr;
        errno = 0;
        const unsigned long value = std::strtoul(cursor, &end, 10);
        if (cursor == end || errno == ERANGE || value > 255) {
            return false;
        }
        parts[i] = static_cast<uint32_t>(value);

        if (i < 3) {
            if (*end != '.') {
                return false;
            }
            cursor = end + 1;
        } else if (*end != '\0') {
            return false;
        }
    }

    outIp = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return true;
}

std::string formatIPv4(uint32_t ip) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (ip >> 24) & 0xFFu, (ip >> 16) & 0xFFu, (ip >> 8) & 0xFFu, ip & 0xFFu);
    return buf;
}

} // namespace net
