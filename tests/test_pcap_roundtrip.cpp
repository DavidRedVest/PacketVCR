#include "PcapReader.h"
#include "PcapWriter.h"

#include <QTemporaryFile>
#include <QTest>

class TestPcapRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void roundTripsPayloadTimestampAndAddresses();
    void handlesEmptyPayload();
    void handlesOddLengthPayload();
    void handlesMaxRealisticTsPayload(); // 1316B = 7x188B MPEG-TS-over-UDP, common real-world size
    void rejectsNonPcapFile();
    void multiplePacketsPreserveOrderAndDeltas();
};

namespace {
QString tempFilePath() {
    QTemporaryFile f;
    f.setAutoRemove(false);
    f.open();
    const QString path = f.fileName();
    f.close();
    return path;
}
} // namespace

void TestPcapRoundtrip::roundTripsPayloadTimestampAndAddresses() {
    const QString path = tempFilePath();

    const std::vector<uint8_t> payload = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01, 0x02, 0x03};
    pcap::FrameParams params;
    params.srcIp = 0xC0A80101; // 192.168.1.1
    params.dstIp = 0xE0010104; // 224.1.1.4
    params.srcPort = 5000;
    params.dstPort = 6010;

    const auto captureTime = std::chrono::system_clock::now();

    {
        pcap::PcapWriter writer;
        QVERIFY(writer.open(path.toStdString()));
        QVERIFY(writer.writePacket(captureTime, params, payload.data(), payload.size()));
        writer.close();
    }

    pcap::PcapReader reader;
    QVERIFY(reader.open(path.toStdString()));
    pcap::PcapReader::Packet packet;
    QVERIFY(reader.nextPacket(packet));

    QCOMPARE(packet.srcIp, params.srcIp);
    QCOMPARE(packet.dstIp, params.dstIp);
    QCOMPARE(packet.srcPort, params.srcPort);
    QCOMPARE(packet.dstPort, params.dstPort);
    QCOMPARE(packet.payload.size(), payload.size());
    QCOMPARE(std::vector<uint8_t>(packet.payload.begin(), packet.payload.end()), payload);

    // pcap timestamps are microsecond-resolution; allow for that truncation.
    const auto deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(packet.timestamp - captureTime).count();
    QVERIFY(std::llabs(deltaUs) < 1);

    pcap::PcapReader::Packet notFound;
    QVERIFY(!reader.nextPacket(notFound)); // exactly one record in the file

    QFile::remove(path);
}

void TestPcapRoundtrip::handlesEmptyPayload() {
    const QString path = tempFilePath();
    pcap::FrameParams params;
    params.srcIp = 1;
    params.dstIp = 2;
    params.srcPort = 1;
    params.dstPort = 2;

    {
        pcap::PcapWriter writer;
        QVERIFY(writer.open(path.toStdString()));
        QVERIFY(writer.writePacket(std::chrono::system_clock::now(), params, nullptr, 0));
        writer.close();
    }

    pcap::PcapReader reader;
    QVERIFY(reader.open(path.toStdString()));
    pcap::PcapReader::Packet packet;
    QVERIFY(reader.nextPacket(packet));
    QCOMPARE(packet.payload.size(), static_cast<size_t>(0));

    QFile::remove(path);
}

void TestPcapRoundtrip::handlesOddLengthPayload() {
    const QString path = tempFilePath();
    const std::vector<uint8_t> payload = {1, 2, 3, 4, 5}; // odd length
    pcap::FrameParams params;
    params.srcIp = 1;
    params.dstIp = 2;
    params.srcPort = 1;
    params.dstPort = 2;

    {
        pcap::PcapWriter writer;
        QVERIFY(writer.open(path.toStdString()));
        QVERIFY(writer.writePacket(std::chrono::system_clock::now(), params, payload.data(), payload.size()));
        writer.close();
    }

    pcap::PcapReader reader;
    QVERIFY(reader.open(path.toStdString()));
    pcap::PcapReader::Packet packet;
    QVERIFY(reader.nextPacket(packet));
    QCOMPARE(std::vector<uint8_t>(packet.payload.begin(), packet.payload.end()), payload);

    QFile::remove(path);
}

void TestPcapRoundtrip::handlesMaxRealisticTsPayload() {
    const QString path = tempFilePath();
    std::vector<uint8_t> payload(1316);
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<uint8_t>(i & 0xFF);
    pcap::FrameParams params;
    params.srcIp = 1;
    params.dstIp = 2;
    params.srcPort = 1;
    params.dstPort = 2;

    {
        pcap::PcapWriter writer;
        QVERIFY(writer.open(path.toStdString()));
        QVERIFY(writer.writePacket(std::chrono::system_clock::now(), params, payload.data(), payload.size()));
        writer.close();
    }

    pcap::PcapReader reader;
    QVERIFY(reader.open(path.toStdString()));
    pcap::PcapReader::Packet packet;
    QVERIFY(reader.nextPacket(packet));
    QCOMPARE(std::vector<uint8_t>(packet.payload.begin(), packet.payload.end()), payload);

    QFile::remove(path);
}

void TestPcapRoundtrip::rejectsNonPcapFile() {
    const QString path = tempFilePath();
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not a pcap file, just some bytes");
    f.close();

    pcap::PcapReader reader;
    QVERIFY(!reader.open(path.toStdString()));
    QVERIFY(!reader.lastError().empty());

    QFile::remove(path);
}

void TestPcapRoundtrip::multiplePacketsPreserveOrderAndDeltas() {
    const QString path = tempFilePath();
    pcap::FrameParams params;
    params.srcIp = 1;
    params.dstIp = 2;
    params.srcPort = 1;
    params.dstPort = 2;

    const auto t0 = std::chrono::system_clock::now();
    const auto t1 = t0 + std::chrono::milliseconds(20);
    const auto t2 = t0 + std::chrono::milliseconds(50);

    {
        pcap::PcapWriter writer;
        QVERIFY(writer.open(path.toStdString()));
        const std::vector<uint8_t> p0 = {0};
        const std::vector<uint8_t> p1 = {1};
        const std::vector<uint8_t> p2 = {2};
        QVERIFY(writer.writePacket(t0, params, p0.data(), p0.size()));
        QVERIFY(writer.writePacket(t1, params, p1.data(), p1.size()));
        QVERIFY(writer.writePacket(t2, params, p2.data(), p2.size()));
        writer.close();
    }

    pcap::PcapReader reader;
    QVERIFY(reader.open(path.toStdString()));
    pcap::PcapReader::Packet a, b, c;
    QVERIFY(reader.nextPacket(a));
    QVERIFY(reader.nextPacket(b));
    QVERIFY(reader.nextPacket(c));
    pcap::PcapReader::Packet extra;
    QVERIFY(!reader.nextPacket(extra));

    QCOMPARE(a.payload[0], static_cast<uint8_t>(0));
    QCOMPARE(b.payload[0], static_cast<uint8_t>(1));
    QCOMPARE(c.payload[0], static_cast<uint8_t>(2));

    const auto deltaAB = std::chrono::duration_cast<std::chrono::milliseconds>(b.timestamp - a.timestamp).count();
    const auto deltaAC = std::chrono::duration_cast<std::chrono::milliseconds>(c.timestamp - a.timestamp).count();
    QCOMPARE(deltaAB, 20LL);
    QCOMPARE(deltaAC, 50LL);

    QFile::remove(path);
}

QTEST_MAIN(TestPcapRoundtrip)
#include "test_pcap_roundtrip.moc"
