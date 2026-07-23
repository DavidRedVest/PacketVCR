#pragma once

#include <cstdint>
#include <string>

namespace net {

// Parses "a.b.c.d" into a host-order uint32_t (e.g. "224.1.1.4" -> 0xE0010104).
// Returns false on malformed input.
bool parseIPv4(const std::string& text, uint32_t& outIp);

// Formats a host-order uint32_t as "a.b.c.d".
std::string formatIPv4(uint32_t ip);

} // namespace net
