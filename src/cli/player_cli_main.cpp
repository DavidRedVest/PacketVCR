#include "Player.h"

#include "IPv4Address.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {
std::atomic<bool> g_stopRequested{false};
void handleSigint(int) {
    g_stopRequested.store(true);
}

uint32_t parseIPv4OrExit(const char* text) {
    uint32_t ip = 0;
    if (!net::parseIPv4(text, ip)) {
        std::fprintf(stderr, "invalid IPv4 address: %s\n", text);
        std::exit(1);
    }
    return ip;
}

unsigned long parseUnsignedOrExit(const char* text, unsigned long minValue, unsigned long maxValue, const char* label) {
    if (*text < '0' || *text > '9') {
        std::fprintf(stderr, "invalid %s: %s\n", label, text);
        std::exit(1);
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (text == end || *end != '\0' || errno == ERANGE || value < minValue || value > maxValue) {
        std::fprintf(stderr, "invalid %s: %s\n", label, text);
        std::exit(1);
    }
    return value;
}

double parseSpeedOrExit(const char* text) {
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(text, &end);
    if (text == end || *end != '\0' || errno == ERANGE || !std::isfinite(value) || value <= 0.0) {
        std::fprintf(stderr, "invalid speed multiplier: %s\n", text);
        std::exit(1);
    }
    return value;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
                      "usage: %s <input.pcap> <dest-ip> <dest-port> [speed] [loop:0|1] [ttl]\n"
                      "example: %s capture.pcap 224.1.1.4 6010 1.0 0 1\n",
                      argv[0], argv[0]);
        return 1;
    }

    core::PlayerConfig config;
    config.inputPath = argv[1];
    config.destIp = parseIPv4OrExit(argv[2]);
    config.destPort = static_cast<uint16_t>(parseUnsignedOrExit(argv[3], 1, 65535, "UDP port"));
    config.speedMultiplier = argc > 4 ? parseSpeedOrExit(argv[4]) : 1.0;
    config.loop = argc > 5 ? parseUnsignedOrExit(argv[5], 0, 1, "loop flag") != 0 : false;
    config.multicastTtl = argc > 6 ? static_cast<uint8_t>(parseUnsignedOrExit(argv[6], 1, 255, "multicast TTL")) : 1;

    core::Player player;
    player.setLogCallback([](const std::string& msg) {
        std::fprintf(stderr, "[player] %s\n", msg.c_str());
    });

    if (!player.start(config)) {
        std::fprintf(stderr, "failed to start player\n");
        return 1;
    }

    std::signal(SIGINT, handleSigint);
    std::fprintf(stderr, "playing... press Ctrl+C to stop\n");

    while (!g_stopRequested.load() && player.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const auto stats = player.stats();
        std::fprintf(stderr, "\r%llu/%llu packets sent, %llu bytes",
                     static_cast<unsigned long long>(stats.currentIndex),
                     static_cast<unsigned long long>(stats.totalPackets),
                     static_cast<unsigned long long>(stats.bytesSent));
        std::fflush(stderr);
    }
    std::fprintf(stderr, "\n");

    player.stop();
    return 0;
}
