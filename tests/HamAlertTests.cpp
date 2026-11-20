#include "hamalert/HamAlertController.h"
#include "hamalert/HamAlertProtocol.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class HamAlertTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesSpotJson();
    void rejectsInvalidAndUnrelatedLines();
    void logsInSetsJsonModeAndReceivesSpots();
    void rejectsEnableWithoutCredentials();
};

void HamAlertTests::parsesSpotJson()
{
    const auto alert = HamAlert::parseAlert(QByteArrayLiteral(
        "{\"fullCallsign\":\"EA8/K1ABC/P\",\"callsign\":\"K1ABC\","
        "\"frequency\":\"14.074000\",\"band\":\"20m\",\"mode\":\"FT8\","
        "\"time\":\"12:34\",\"entity\":\"Canary Islands\",\"spotter\":\"W1XYZ\","
        "\"source\":\"pskreporter\",\"triggerComment\":\"New DXCC\"}"));
    QVERIFY(alert);
    QCOMPARE(alert->fullCallsign, QStringLiteral("EA8/K1ABC/P"));
    QCOMPARE(alert->callsign, QStringLiteral("K1ABC"));
    QCOMPARE(alert->frequency, QStringLiteral("14.074000"));
    QCOMPARE(alert->band, QStringLiteral("20m"));
    QCOMPARE(alert->mode, QStringLiteral("FT8"));
    QCOMPARE(alert->entity, QStringLiteral("Canary Islands"));
    QCOMPARE(alert->triggerComment, QStringLiteral("New DXCC"));
}

void HamAlertTests::rejectsInvalidAndUnrelatedLines()
{
    QVERIFY(!HamAlert::parseAlert({}));
    QVERIFY(!HamAlert::parseAlert(QByteArrayLiteral("Not a spot")));
    QVERIFY(!HamAlert::parseAlert(QByteArrayLiteral("{}")));
    QVERIFY(!HamAlert::parseAlert(QByteArrayLiteral("{\"frequency\":\"14.074\"}")));
    QVERIFY(!HamAlert::parseAlert(QByteArray(65537, 'x')));
}

void HamAlertTests::logsInSetsJsonModeAndReceivesSpots()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    HamAlertController controller(QStringLiteral("127.0.0.1"), server.serverPort());
    controller.setCredentials(QStringLiteral("N0CALL"), QStringLiteral("test-telnet-password"));
    QList<HamAlert::Alert> received;
    connect(&controller, &HamAlertController::alertReceived, this,
            [&received](const HamAlert::Alert &alert) { received.append(alert); });

    QVERIFY(controller.setEnabled(true));
    QTRY_VERIFY(server.hasPendingConnections());
    QTcpSocket *client = server.nextPendingConnection();
    QVERIFY(client);

    client->write(QByteArrayLiteral("HamAlert Telnet/Cluster emulation server\r\nlogin: "));
    client->flush();
    QTRY_VERIFY(client->bytesAvailable() > 0);
    QCOMPARE(client->readAll(), QByteArrayLiteral("N0CALL\r\n"));

    client->write(QByteArrayLiteral("password: "));
    client->flush();
    QTRY_VERIFY(client->bytesAvailable() > 0);
    QCOMPARE(client->readAll(), QByteArrayLiteral("test-telnet-password\r\n"));

    client->write(QByteArrayLiteral("Hello N0CALL, this is HamAlert\r\nN0CALL de HamAlert>\r\n"));
    client->flush();
    QTRY_VERIFY(client->bytesAvailable() > 0);
    QCOMPARE(client->readAll(), QByteArrayLiteral("set/json\r\n"));

    client->write(QByteArrayLiteral(
        "Operation successful\r\n"
        "{\"fullCallsign\":\"K1ABC\",\"callsign\":\"K1ABC\","
        "\"frequency\":\"14.074000\",\"mode\":\"FT8\"}\r\n"));
    client->flush();
    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(received.first().fullCallsign, QStringLiteral("K1ABC"));
    QCOMPARE(received.first().frequency, QStringLiteral("14.074000"));

    QVERIFY(controller.setEnabled(false));
    client->deleteLater();
}

void HamAlertTests::rejectsEnableWithoutCredentials()
{
    HamAlertController controller(QStringLiteral("127.0.0.1"), 1);
    QVERIFY(!controller.setEnabled(true));
    QVERIFY(!controller.isEnabled());
    QVERIFY(!controller.errorString().isEmpty());
}

QTEST_GUILESS_MAIN(HamAlertTests)
#include "HamAlertTests.moc"
