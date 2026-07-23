#include "FakeHeaders.h"

#include <QTest>
#include <cstring>

// Well-known worked example (widely used in networking textbooks/tutorials)
// of an IPv4 header checksum computation, used here as an independent,
// known-good vector rather than trusting our own implementation to check
// itself.
class TestChecksum : public QObject {
    Q_OBJECT

private slots:
    void knownVectorWithZeroedChecksumField();
    void completeHeaderSumsToZero();
    void emptyInput();
    void oddLengthInput();
};

namespace {
const uint8_t kSampleHeaderChecksumZeroed[20] = {
    0x45, 0x00, 0x00, 0x3c, 0x1c, 0x46, 0x40, 0x00, 0x40, 0x06,
    0x00, 0x00, 0xac, 0x10, 0x0a, 0x63, 0xac, 0x10, 0x0a, 0x0c,
};
} // namespace

void TestChecksum::knownVectorWithZeroedChecksumField() {
    const uint16_t checksum = pcap::internetChecksum(kSampleHeaderChecksumZeroed, sizeof(kSampleHeaderChecksumZeroed));
    QCOMPARE(checksum, static_cast<uint16_t>(0xb1e6));
}

void TestChecksum::completeHeaderSumsToZero() {
    uint8_t header[20];
    memcpy(header, kSampleHeaderChecksumZeroed, sizeof(header));
    header[10] = 0xb1;
    header[11] = 0xe6;
    // A correct one's-complement checksum over data that already includes
    // the correct checksum field sums to 0 (mod 0xFFFF's-complement).
    QCOMPARE(pcap::internetChecksum(header, sizeof(header)), static_cast<uint16_t>(0x0000));
}

void TestChecksum::emptyInput() {
    QCOMPARE(pcap::internetChecksum(nullptr, 0), static_cast<uint16_t>(0xffff));
}

void TestChecksum::oddLengthInput() {
    const uint8_t data[3] = {0x12, 0x34, 0x56};
    // 0x1234 + 0x5600 (odd trailing byte zero-padded as high byte) = 0x6834 -> ~ = 0x97cb
    QCOMPARE(pcap::internetChecksum(data, sizeof(data)), static_cast<uint16_t>(0x97cb));
}

QTEST_MAIN(TestChecksum)
#include "test_checksum.moc"
