#include "PcapWriter.h"
#include "Player.h"
#include "PlatformSocket.h"

#include <QTemporaryFile>
#include <QTest>

#include <thread>
#include <vector>

namespace {
constexpr uint32_t kLoopbackIp = 0x7F000001; // 127.0.0.1

QString tempPcapPath() {
    QTemporaryFile f;
    f.setAutoRemove(false);
    f.open();
    const QString path = f.fileName();
    f.close();
    return path;
}

// Writes `count` packets spaced `stepMs` apart; each payload's first byte is
// its index, so a receiver can verify ordering.
void writeTestPcap(const QString& path, int count, int stepMs) {
    pcap::PcapWriter writer;
    QVERIFY(writer.open(path.toStdString()));
    pcap::FrameParams params;
    params.srcIp = kLoopbackIp;
    params.dstIp = kLoopbackIp;
    params.srcPort = 9000;
    params.dstPort = 9001;

    auto t = std::chrono::system_clock::now();
    for (int i = 0; i < count; ++i) {
        std::vector<uint8_t> payload(16, 0);
        payload[0] = static_cast<uint8_t>(i);
        QVERIFY(writer.writePacket(t, params, payload.data(), payload.size()));
        t += std::chrono::milliseconds(stepMs);
    }
    writer.close();
}

struct ReceivedPacket {
    std::chrono::steady_clock::time_point arrival;
    uint8_t index;
};

// Receives up to `expectedCount` packets (or until `overallTimeoutMs`
// elapses), recording arrival time + payload index for each.
std::vector<ReceivedPacket> receivePackets(uint16_t port, int expectedCount, int overallTimeoutMs) {
    net::PlatformSocket sock;
    sock.bindAny(port);

    std::vector<ReceivedPacket> received;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(overallTimeoutMs);
    uint8_t buf[2048];
    while (static_cast<int>(received.size()) < expectedCount && std::chrono::steady_clock::now() < deadline) {
        size_t len = 0;
        uint32_t srcIp = 0;
        uint16_t srcPort = 0;
        if (sock.recvFrom(buf, sizeof(buf), 100, len, srcIp, srcPort) == net::RecvResult::Data && len > 0) {
            received.push_back({std::chrono::steady_clock::now(), buf[0]});
        }
    }
    return received;
}
} // namespace

class TestPlayerTiming : public QObject {
    Q_OBJECT

private slots:
    void noCumulativeDriftOverManyPackets();
    void pauseResumeShiftsTimingWithoutLoss();
    void speedMultiplierScalesTotalDuration();
};

void TestPlayerTiming::noCumulativeDriftOverManyPackets() {
    const QString path = tempPcapPath();
    const int count = 25;
    const int stepMs = 20;
    writeTestPcap(path, count, stepMs);

    const uint16_t port = 19001;
    std::thread receiver;
    std::vector<ReceivedPacket> received;
    receiver = std::thread([&] { received = receivePackets(port, count, 5000); });

    // Give the receiver a moment to bind before packets start flowing.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    core::Player player;
    core::PlayerConfig config;
    config.inputPath = path.toStdString();
    config.destIp = kLoopbackIp;
    config.destPort = port;
    config.speedMultiplier = 1.0;
    QVERIFY(player.start(config));

    receiver.join();
    player.stop();

    QCOMPARE(static_cast<int>(received.size()), count);
    for (int i = 0; i < count; ++i) {
        QCOMPARE(static_cast<int>(received[i].index), i); // order preserved
    }

    const auto actualTotalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(received.back().arrival - received.front().arrival).count();
    const auto expectedTotalMs = static_cast<int64_t>(stepMs) * (count - 1);
    // Anti-drift scheduling should keep even the *last* packet close to its
    // absolute deadline -- not just "average" delay -- so a generous but
    // bounded per-run-tolerance here (not proportional to packet count) is
    // exactly what distinguishes this from a naive accumulating-sleep bug.
    QVERIFY2(std::llabs(actualTotalMs - expectedTotalMs) < 60,
             qPrintable(QString("expected ~%1ms, got %2ms").arg(expectedTotalMs).arg(actualTotalMs)));

    QFile::remove(path);
}

void TestPlayerTiming::pauseResumeShiftsTimingWithoutLoss() {
    const QString path = tempPcapPath();
    const int count = 10;
    const int stepMs = 30;
    writeTestPcap(path, count, stepMs);

    const uint16_t port = 19002;
    std::thread receiver;
    std::vector<ReceivedPacket> received;
    receiver = std::thread([&] { received = receivePackets(port, count, 5000); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    core::Player player;
    core::PlayerConfig config;
    config.inputPath = path.toStdString();
    config.destIp = kLoopbackIp;
    config.destPort = port;
    config.speedMultiplier = 1.0;
    QVERIFY(player.start(config));

    // Let a few packets go out, then pause mid-stream for a known duration.
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    player.pause();
    QVERIFY(player.isPaused());
    const auto pauseDurationMs = 300;
    const auto pauseStart = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseDurationMs));
    // sleep_for only guarantees *at least* pauseDurationMs -- on a loaded/virtualized
    // CI machine it can overshoot well beyond that, so measure what actually elapsed
    // rather than assuming the nominal value when computing the expected total below.
    const auto actualPauseMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - pauseStart).count();
    player.resume();
    QVERIFY(!player.isPaused());

    receiver.join();
    player.stop();

    QCOMPARE(static_cast<int>(received.size()), count); // no packets lost/dropped by pausing
    for (int i = 0; i < count; ++i) {
        QCOMPARE(static_cast<int>(received[i].index), i);
    }

    const auto actualTotalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(received.back().arrival - received.front().arrival).count();
    const auto expectedTotalMs = static_cast<int64_t>(stepMs) * (count - 1) + actualPauseMs;
    QVERIFY2(std::llabs(actualTotalMs - expectedTotalMs) < 60,
             qPrintable(QString("expected ~%1ms (incl. pause), got %2ms").arg(expectedTotalMs).arg(actualTotalMs)));

    QFile::remove(path);
}

void TestPlayerTiming::speedMultiplierScalesTotalDuration() {
    const QString path = tempPcapPath();
    const int count = 15;
    const int stepMs = 20;
    writeTestPcap(path, count, stepMs);

    const uint16_t port = 19003;
    std::thread receiver;
    std::vector<ReceivedPacket> received;
    receiver = std::thread([&] { received = receivePackets(port, count, 5000); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    core::Player player;
    core::PlayerConfig config;
    config.inputPath = path.toStdString();
    config.destIp = kLoopbackIp;
    config.destPort = port;
    config.speedMultiplier = 2.0; // twice as fast -> half the total duration
    QVERIFY(player.start(config));

    receiver.join();
    player.stop();

    QCOMPARE(static_cast<int>(received.size()), count);

    const auto actualTotalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(received.back().arrival - received.front().arrival).count();
    const auto expectedTotalMs = (static_cast<int64_t>(stepMs) * (count - 1)) / 2;
    QVERIFY2(std::llabs(actualTotalMs - expectedTotalMs) < 40,
             qPrintable(QString("expected ~%1ms at 2x, got %2ms").arg(expectedTotalMs).arg(actualTotalMs)));

    QFile::remove(path);
}

QTEST_MAIN(TestPlayerTiming)
#include "test_player_timing.moc"
