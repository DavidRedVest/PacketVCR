#include "IPv4Address.h"

#include <QTest>

class TestIPv4Address : public QObject {
    Q_OBJECT

private slots:
    void parsesValidAddress();
    void rejectsMalformedInput();
};

void TestIPv4Address::parsesValidAddress() {
    uint32_t ip = 0;
    QVERIFY(net::parseIPv4("224.1.1.4", ip));
    QCOMPARE(ip, static_cast<uint32_t>(0xE0010104));
    QCOMPARE(QString::fromStdString(net::formatIPv4(ip)), QString("224.1.1.4"));
}

void TestIPv4Address::rejectsMalformedInput() {
    uint32_t ip = 0;
    QVERIFY(!net::parseIPv4("", ip));
    QVERIFY(!net::parseIPv4("1.2.3", ip));
    QVERIFY(!net::parseIPv4("1.2.3.4abc", ip));
    QVERIFY(!net::parseIPv4("1.2.3.256", ip));
    QVERIFY(!net::parseIPv4("1.2..4", ip));
    QVERIFY(!net::parseIPv4(" 1.2.3.4", ip));
    QVERIFY(!net::parseIPv4("+1.2.3.4", ip));
}

QTEST_MAIN(TestIPv4Address)
#include "test_ipv4_address.moc"
