#include "PcapWriter.h"
#include "PcapFormat.h"

namespace pcap {

PcapWriter::~PcapWriter() {
    close();
}

bool PcapWriter::open(const std::string& path, uint32_t snapLen) {
    close();
    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        return false;
    }

    uint8_t header[kGlobalHeaderSize];
    putLE32(header + 0, kMagicMicros);
    putLE16(header + 4, kVersionMajor);
    putLE16(header + 6, kVersionMinor);
    putLE32(header + 8, 0);       // thiszone
    putLE32(header + 12, 0);      // sigfigs
    putLE32(header + 16, snapLen);
    putLE32(header + 20, kLinkTypeEthernet);

    file_.write(reinterpret_cast<const char*>(header), kGlobalHeaderSize);
    if (!file_) {
        file_.close();
        return false;
    }
    return true;
}

bool PcapWriter::writePacket(std::chrono::system_clock::time_point captureTime,
                              const FrameParams& params,
                              const uint8_t* payload, size_t payloadLen) {
    if (!file_.is_open()) {
        return false;
    }

    const size_t frameLen = syntheticFrameSize(payloadLen);
    scratch_.resize(frameLen);
    const size_t written = buildSyntheticFrame(scratch_.data(), scratch_.size(), params, payload, payloadLen);
    if (written == 0) {
        return false;
    }

    const auto sinceEpoch = captureTime.time_since_epoch();
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(sinceEpoch);
    const auto usecs = std::chrono::duration_cast<std::chrono::microseconds>(sinceEpoch - secs);

    uint8_t recordHeader[kRecordHeaderSize];
    putLE32(recordHeader + 0, static_cast<uint32_t>(secs.count()));
    putLE32(recordHeader + 4, static_cast<uint32_t>(usecs.count()));
    putLE32(recordHeader + 8, static_cast<uint32_t>(written));
    putLE32(recordHeader + 12, static_cast<uint32_t>(written));

    file_.write(reinterpret_cast<const char*>(recordHeader), kRecordHeaderSize);
    file_.write(reinterpret_cast<const char*>(scratch_.data()), static_cast<std::streamsize>(written));
    return static_cast<bool>(file_);
}

void PcapWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

void PcapWriter::close() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

} // namespace pcap
