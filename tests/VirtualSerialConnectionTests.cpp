/*
 * GS-232B Open GUI - rotator controller GUI.
 * Copyright (C) 2026 BG5GEB <bg5geb@misakanet.team>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "ControlPanel.h"
#include "MainWindow.h"
#include "virtual_serial/VirtualSerialBridge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QPointer>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

class VirtualSerialConnectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void failedConnectionDoesNotCreatePort();
#ifdef Q_OS_UNIX
    void forwardsSerialCommandsAsRawBinaryFrames();
    void forwardsFragmentedTextAndBinaryReplies();
    void forwardsSensitiveCommandsWithoutEcho();
    void reportsManagementQueries();
    void doesNotSendDirectCommandsOrPoll();
    void disconnectAndReconnect_data();
    void disconnectAndReconnect();
    void windowDestructionClosesBothSides();
#else
    void invalidPortPair_data();
    void invalidPortPair();
#endif

private:
    bool connectDevice();
    QTemporaryDir m_settingsDirectory;
#ifdef Q_OS_UNIX
    bool openClient();
    QByteArray readReply(qsizetype expectedSize);
    QFile m_client;
#endif
    MainWindow *m_window = nullptr;
    ConnectionDialog *m_connection = nullptr;
    Communication::Transport *m_transport = nullptr;
    VirtualSerialBridge *m_bridge = nullptr;
    QWebSocketServer *m_server = nullptr;
    QPointer<QWebSocket> m_peer;
    QList<QByteArray> m_binaryFrames;
    QStringList m_textFrames;
};

void VirtualSerialConnectionTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDirectory.path());
}

void VirtualSerialConnectionTests::init()
{
    m_server = new QWebSocketServer(QStringLiteral("virtual-serial-test"),
                                    QWebSocketServer::NonSecureMode);
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    connect(m_server, &QWebSocketServer::newConnection, this, [this] {
        m_peer = m_server->nextPendingConnection();
        m_peer->setParent(m_server);
        connect(m_peer, &QWebSocket::binaryMessageReceived, this,
                [this](const QByteArray &bytes) { m_binaryFrames.append(bytes); });
        connect(m_peer, &QWebSocket::textMessageReceived, this,
                [this](const QString &text) { m_textFrames.append(text); });
    });
    m_window = new MainWindow;
    m_connection = m_window->findChild<ConnectionDialog *>();
    m_transport = m_window->findChild<Communication::Transport *>();
    m_bridge = m_window->findChild<VirtualSerialBridge *>();
    QVERIFY(m_connection);
    QVERIFY(m_transport);
    QVERIFY(m_bridge);
    m_window->findChild<QRadioButton *>(QStringLiteral("networkMode"))->setChecked(true);
    m_window->findChild<QCheckBox *>(QStringLiteral("virtualCom"))->setChecked(true);
    m_window->findChild<QLineEdit *>(QStringLiteral("websocketAddress"))
        ->setText(QStringLiteral("ws://127.0.0.1:%1").arg(m_server->serverPort()));
}

void VirtualSerialConnectionTests::cleanup()
{
#ifdef Q_OS_UNIX
    m_client.close();
#endif
    delete m_window;
    m_window = nullptr;
    delete m_server;
    m_server = nullptr;
    m_peer.clear();
    m_binaryFrames.clear();
    m_textFrames.clear();
}

bool VirtualSerialConnectionTests::connectDevice()
{
    m_connection->connectRequested();
    return QTest::qWaitFor([this] { return m_peer && m_connection->isConnected(); }, 3000);
}

void VirtualSerialConnectionTests::failedConnectionDoesNotCreatePort()
{
    m_server->close();
    QSignalSpy disconnected(m_transport, &Communication::Transport::disconnected);
    m_connection->connectRequested();
    QTRY_COMPARE(disconnected.size(), 1);
    QVERIFY(!m_connection->isConnected());
    QVERIFY(!m_transport->isOpen());
    QVERIFY(!m_bridge->isRunning());
    QVERIFY(m_bridge->clientPortName().isEmpty());
}

#ifdef Q_OS_UNIX
bool VirtualSerialConnectionTests::openClient()
{
    const QByteArray path = m_bridge->clientPortName().toLocal8Bit();
    const int fd = ::open(path.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return false;
    if (!m_client.open(fd, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return false;
    }
    return true;
}

QByteArray VirtualSerialConnectionTests::readReply(qsizetype expectedSize)
{
    QByteArray received;
    const bool complete = QTest::qWaitFor([&] {
        char buffer[1024];
        const ssize_t count = ::read(m_client.handle(), buffer, sizeof(buffer));
        if (count > 0)
            received.append(buffer, static_cast<qsizetype>(count));
        return received.size() >= expectedSize;
    }, 3000);
    if (!complete)
        QTest::qFail("Timed out waiting for the virtual serial reply", __FILE__, __LINE__);
    return received;
}

void VirtualSerialConnectionTests::forwardsSerialCommandsAsRawBinaryFrames()
{
    QVERIFY(connectDevice());
    QVERIFY(m_bridge->isRunning());
    QCOMPARE(m_peer->requestUrl().path(), QStringLiteral("/ws/gs232b"));
    QVERIFY(openClient());
    QCOMPARE(::write(m_client.handle(), "M1", 2), 2);
    QTest::qWait(30);
    QVERIFY(m_binaryFrames.isEmpty());
    QCOMPARE(::write(m_client.handle(), "23\r\nC2\rZ", 8), 8);
    QTRY_COMPARE(m_binaryFrames.size(), 2);
    QCOMPARE(m_binaryFrames.at(0), QByteArrayLiteral("M123\r"));
    QCOMPARE(m_binaryFrames.at(1), QByteArrayLiteral("C2\r"));

    const QByteArray bytes = QByteArray::fromHex("00ff0d");
    QCOMPARE(::write(m_client.handle(), bytes.constData(), bytes.size()), bytes.size());
    QTRY_COMPARE(m_binaryFrames.size(), 3);
    QCOMPARE(m_binaryFrames.at(2), QByteArrayLiteral("Z") + bytes);
    QVERIFY(m_textFrames.isEmpty());
}

void VirtualSerialConnectionTests::forwardsFragmentedTextAndBinaryReplies()
{
    QVERIFY(connectDevice());
    QVERIFY(openClient());
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(QStringLiteral("AZ=123 EL="));
    QTRY_COMPARE(received.size(), 1);
    char byte;
    QVERIFY(::read(m_client.handle(), &byte, 1) < 0);

    m_peer->sendBinaryMessage(QByteArrayLiteral("045\r\nOK\r"));
    const QByteArray expected = QByteArrayLiteral("AZ=123 EL=045\rOK\r");
    QCOMPARE(readReply(expected.size()), expected);
    m_peer->sendTextMessage(QStringLiteral("\nTAIL\r"));
    QCOMPARE(readReply(5), QByteArrayLiteral("TAIL\r"));
}

void VirtualSerialConnectionTests::doesNotSendDirectCommandsOrPoll()
{
    QVERIFY(connectDevice());
    m_window->findChild<ControlPanel *>()->commandRequested(QStringLiteral("M123"));
    QTest::qWait(1200);
    QVERIFY(m_binaryFrames.isEmpty());
    QVERIFY(m_textFrames.isEmpty());
}

void VirtualSerialConnectionTests::forwardsSensitiveCommandsWithoutEcho()
{
    QVERIFY(connectDevice());
    QVERIFY(openClient());
    QSignalSpy sent(m_transport, &Communication::Transport::bytesSent);
    const QByteArray command = QByteArrayLiteral("!SESSION AUTH {\"password\":\"test-secret\"}\r");
    QCOMPARE(::write(m_client.handle(), command.constData(), command.size()), command.size());
    QTRY_COMPARE(m_binaryFrames, QList<QByteArray>{command});
    QVERIFY(sent.isEmpty());
}

void VirtualSerialConnectionTests::reportsManagementQueries()
{
    QVERIFY(connectDevice());
    QVERIFY(openClient());
    QSignalSpy sent(m_transport, &Communication::Transport::bytesSent);
    const QByteArray command = QByteArrayLiteral("!NET GET\r");
    QCOMPARE(::write(m_client.handle(), command.constData(), command.size()), command.size());
    QTRY_COMPARE(m_binaryFrames, QList<QByteArray>{command});
    QCOMPARE(sent.size(), 1);
    QCOMPARE(sent.first().first().toByteArray(), command);
}

void VirtualSerialConnectionTests::disconnectAndReconnect_data()
{
    QTest::addColumn<int>("closingSide");
    QTest::newRow("user-disconnect") << 0;
    QTest::newRow("websocket-disconnect") << 1;
    QTest::newRow("endpoint-stop") << 2;
}

void VirtualSerialConnectionTests::disconnectAndReconnect()
{
    QFETCH(int, closingSide);
    QVERIFY(connectDevice());
    QVERIFY(openClient());
    QCOMPARE(::write(m_client.handle(), "STALE", 5), 5);
    QTest::qWait(30);
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(QStringLiteral("OLD"));
    QTRY_COMPARE(received.size(), 1);

    if (closingSide == 0)
        m_connection->disconnectRequested();
    else if (closingSide == 1)
        m_peer->close();
    else
        m_bridge->stop();
    QTRY_VERIFY(!m_connection->isConnected());
    QVERIFY(!m_transport->isOpen());
    QVERIFY(!m_bridge->isRunning());
    QVERIFY(m_bridge->clientPortName().isEmpty());
    QTRY_COMPARE(m_peer->state(), QAbstractSocket::UnconnectedState);
    m_client.close();
    m_peer.clear();

    QVERIFY(connectDevice());
    QVERIFY(openClient());
    QCOMPARE(::write(m_client.handle(), "C2\r", 3), 3);
    QTRY_COMPARE(m_binaryFrames, QList<QByteArray>{QByteArrayLiteral("C2\r")});
    m_peer->sendTextMessage(QStringLiteral("AZ=010 EL=020\r"));
    QCOMPARE(readReply(14), QByteArrayLiteral("AZ=010 EL=020\r"));
    QVERIFY(m_textFrames.isEmpty());
}

void VirtualSerialConnectionTests::windowDestructionClosesBothSides()
{
    QVERIFY(connectDevice());
    QVERIFY(openClient());
    QPointer<VirtualSerialBridge> bridge(m_bridge);
    delete m_window;
    m_window = nullptr;
    QVERIFY(bridge.isNull());
    QTRY_COMPARE(m_peer->state(), QAbstractSocket::UnconnectedState);
}
#else
void VirtualSerialConnectionTests::invalidPortPair_data()
{
    QTest::addColumn<QString>("applicationPort");
    QTest::addColumn<QString>("clientPort");
    QTest::newRow("missing-pair") << QString() << QString();
    QTest::newRow("same-port") << QStringLiteral("COM8") << QStringLiteral("com8");
    QTest::newRow("unavailable-port") << QStringLiteral("COM65534") << QStringLiteral("COM65535");
}

void VirtualSerialConnectionTests::invalidPortPair()
{
    QFETCH(QString, applicationPort);
    QFETCH(QString, clientPort);
    m_window->findChild<QComboBox *>(QStringLiteral("virtualApplicationPort"))
        ->setEditText(applicationPort);
    m_window->findChild<QComboBox *>(QStringLiteral("virtualClientPort"))->setEditText(clientPort);
    bool reportedConnected = false;
    QObject stateObserver;
    connect(m_connection, &ConnectionDialog::stateChanged, &stateObserver,
            [&] { reportedConnected = reportedConnected || m_connection->isConnected(); });
    QSignalSpy error(m_bridge, &VirtualSerialBridge::errorOccurred);
    m_connection->connectRequested();
    QTRY_VERIFY(!error.isEmpty());
    QVERIFY(!reportedConnected);
    QVERIFY(!m_connection->isConnected());
    QVERIFY(!m_transport->isOpen());
    QVERIFY(!m_bridge->isRunning());
    QVERIFY(m_binaryFrames.isEmpty());
    QVERIFY(m_textFrames.isEmpty());
}
#endif

QTEST_MAIN(VirtualSerialConnectionTests)
#include "VirtualSerialConnectionTests.moc"
