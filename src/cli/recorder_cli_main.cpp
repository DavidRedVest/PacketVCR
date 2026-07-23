#include "Recorder.h"

#include "IPv4Address.h"

#include <atomic>
#include <chrono>
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
} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
                      "usage: %s <group-or-bind-ip> <port> <output.pcap> [local-interface-ip]\n"
                      "example: %s 224.1.1.4 6010 capture.pcap\n",
                      argv[0], argv[0]);
        return 1;
    }

    core::RecorderConfig config;
    config.bindOrGroupIp = parseIPv4OrExit(argv[1]);
    config.port = static_cast<uint16_t>(std::atoi(argv[2]));
    config.outputPath = argv[3];
    config.localInterfaceIp = argc > 4 ? parseIPv4OrExit(argv[4]) : 0;

    core::Recorder recorder;
    recorder.setLogCallback([](const std::string& msg) {
        std::fprintf(stderr, "[recorder] %s\n", msg.c_str());
    });

    if (!recorder.start(config)) {
        std::fprintf(stderr, "failed to start recorder\n");
        return 1;
    }

    std::signal(SIGINT, handleSigint);
    std::fprintf(stderr, "recording... press Ctrl+C to stop\n");

    while (!g_stopRequested.load() && recorder.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const auto stats = recorder.stats();
        std::fprintf(stderr, "\rpackets=%llu bytes=%llu", static_cast<unsigned long long>(stats.packetCount),
                     static_cast<unsigned long long>(stats.byteCount));
        std::fflush(stderr);
    }
    std::fprintf(stderr, "\n");

    recorder.stop();
    const auto finalStats = recorder.stats();
    std::fprintf(stderr, "done: %llu packets, %llu bytes\n", static_cast<unsigned long long>(finalStats.packetCount),
                 static_cast<unsigned long long>(finalStats.byteCount));
    return 0;
}
