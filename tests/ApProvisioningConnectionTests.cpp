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
#include "MainWindow.h"
#include "ManagementKeyDialog.h"
#include "WifiProvisioningDialog.h"

#include <QAction>
#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest>

#include <algorithm>

#ifdef Q_OS_UNIX
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#endif

class ApProvisioningConnectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void networkStartUsesBareCommand();
    void wifiPollingReportsEveryRequest_data();
    void wifiPollingReportsEveryRequest();
    void wifiRejectsInvalidConfigurationIds_data();
    void wifiRejectsInvalidConfigurationIds();
    void ipQueryDoesNotRequireAuthentication();
    void ipQueryPreservesWifiTrial();
    void failedAuthenticationDoesNotStartProvisioning();
    void cancelledAuthenticationDoesNotStartProvisioning();
#ifdef Q_OS_UNIX
    void serialStartUsesBareCommand();
#endif

private:
    bool connectDevice();
    void submitPassword();

    QTemporaryDir m_settingsDirectory;
    MainWindow *m_window = nullptr;
    ConnectionDialog *m_connection = nullptr;
    Communication::Transport *m_transport = nullptr;
    QAction *m_start = nullptr;
    QWebSocketServer *m_server = nullptr;
    QPointer<QWebSocket> m_peer;
    QStringList m_commands;
};

void ApProvisioningConnectionTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDirectory.path());
}

void ApProvisioningConnectionTests::init()
{
    m_server = new QWebSocketServer(QStringLiteral("provisioning-test"),
                                  QWebSocketServer::NonSecureMode);
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    connect(m_server, &QWebSocketServer::newConnection, this, [this] {
        m_peer = m_server->nextPendingConnection();
        m_peer->setParent(m_server);
        connect(m_peer, &QWebSocket::textMessageReceived, this,
                [this](const QString &message) { m_commands.append(message); });
    });

    m_window = new MainWindow;
    m_connection = m_window->findChild<ConnectionDialog *>();
    m_transport = m_window->findChild<Communication::Transport *>();
    m_start = m_window->findChild<QAction *>(QStringLiteral("startApProvisioningAction"));
    QVERIFY(m_connection);
    QVERIFY(m_transport);
    QVERIFY(m_start);
    m_window->findChild<QRadioButton *>(QStringLiteral("networkMode"))->setChecked(true);
    m_window->findChild<QLineEdit *>(QStringLiteral("websocketAddress"))
        ->setText(QStringLiteral("ws://127.0.0.1:%1").arg(m_server->serverPort()));
}

void ApProvisioningConnectionTests::cleanup()
{
    delete m_window;
    m_window = nullptr;
    delete m_server;
    m_server = nullptr;
    m_commands.clear();
}

bool ApProvisioningConnectionTests::connectDevice()
{
    m_connection->connectRequested();
    return QTest::qWaitFor([this] { return m_peer && m_connection->isConnected(); }, 3000);
}

void ApProvisioningConnectionTests::submitPassword()
{
    auto *dialog = m_window->findChild<ManagementKeyDialog *>();
    QVERIFY(dialog);
    dialog->findChild<QLineEdit *>(QStringLiteral("managementPassword"))
        ->setText(QStringLiteral("test-password"));
    dialog->accept();
}

void ApProvisioningConnectionTests::networkStartUsesBareCommand()
{
    QVERIFY(connectDevice());
    QVERIFY(m_start->isEnabled());
    m_start->trigger();
    submitPassword();
    QTRY_VERIFY(m_commands.contains(
        QStringLiteral("!SESSION AUTH {\"password\":\"test-password\"}\r")));
    QVERIFY(m_commands.filter(QStringLiteral("!PROV START")).isEmpty());

    m_peer->sendTextMessage(QStringLiteral("!{\"cmd\":\"SESSION AUTH\",\"ok\":true,\"data\":{}}\r\n"));
    QTRY_COMPARE(m_commands.filter(QStringLiteral("!PROV START")),
                 QStringList{QStringLiteral("!PROV START\r")});

    const QJsonObject data{{"state", "window"}, {"remaining_s", 137}};
    const QJsonObject response{{"cmd", "PROV START"}, {"ok", true}, {"data", data}};
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(QLatin1Char('!') +
                           QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)) +
                           QStringLiteral("\r\n"));
    QTRY_COMPARE(received.size(), 1);

    m_start->trigger();
    const QStringList expected{QStringLiteral("!PROV START\r"), QStringLiteral("!PROV START\r")};
    QTRY_COMPARE(m_commands.filter(QStringLiteral("!PROV START")), expected);
    QCOMPARE(m_commands.filter(QStringLiteral("!SESSION AUTH")).size(), 1);
}

void ApProvisioningConnectionTests::wifiPollingReportsEveryRequest_data()
{
    QTest::addColumn<qint64>("candidate");
    QTest::addColumn<qint64>("revision");
    QTest::addColumn<qint64>("trial");
    QTest::newRow("small-ids") << qint64(7) << qint64(2) << qint64(11);
    QTest::newRow("wide-ids") << qint64(4294967297) << qint64(4294967298) << qint64(4294967299);
    QTest::newRow("largest-exact-ids") << qint64(9007199254740991) << qint64(9007199254740990)
                                       << qint64(9007199254740989);
}

void ApProvisioningConnectionTests::wifiPollingReportsEveryRequest()
{
    QFETCH(qint64, candidate);
    QFETCH(qint64, revision);
    QFETCH(qint64, trial);
    QSignalSpy sent(m_transport, &Communication::Transport::bytesSent);
    QVERIFY(connectDevice());
    connect(m_peer, &QWebSocket::textMessageReceived, this, [this, candidate, revision, trial](const QString &message) {
        QString command;
        QJsonObject data;
        if (message.startsWith(QStringLiteral("!SESSION AUTH "))) {
            command = QStringLiteral("SESSION AUTH");
        } else if (message == QStringLiteral("!CFG BEGIN\r")) {
            command = QStringLiteral("CFG BEGIN");
            data = {{"candidate_id", candidate}, {"base_revision", revision}};
        } else if (message.startsWith(QStringLiteral("!NET SET "))) {
            command = QStringLiteral("NET SET");
        } else if (message == QStringLiteral("!CFG CHECK\r")) {
            command = QStringLiteral("CFG CHECK");
        } else if (message.startsWith(QStringLiteral("!CFG APPLY "))) {
            command = QStringLiteral("CFG APPLY");
            data = {{"trial_id", trial}};
        } else if (message == QStringLiteral("!NET GET\r")) {
            command = QStringLiteral("NET GET");
            data = {{"runtime", QJsonObject{{"wifi", QJsonObject{{"connected", false}}}}}};
        } else {
            return;
        }
        const QJsonObject response{{"cmd", command}, {"ok", true}, {"data", data}};
        m_peer->sendTextMessage(QLatin1Char('!') +
                               QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)) +
                               QStringLiteral("\r\n"));
    });

    m_window->findChild<QAction *>(QStringLiteral("wifiAction"))->trigger();
    submitPassword();
    QTRY_VERIFY(m_window->findChild<WifiProvisioningDialog *>());
    m_window->findChild<WifiProvisioningDialog *>()->configurationRequested(
        QStringLiteral("test-wifi"), QStringLiteral("wifi-secret"));
    QTRY_VERIFY_WITH_TIMEOUT(m_commands.count(QStringLiteral("!NET GET\r")) >= 3, 5000);

    QList<QByteArray> reported;
    for (const auto &arguments : sent)
        reported.append(arguments.first().toByteArray());
    QCOMPARE(reported.count(QByteArrayLiteral("!NET GET\r")),
             m_commands.count(QStringLiteral("!NET GET\r")));
    QVERIFY(reported.contains(QByteArrayLiteral("!ROTOR GET\r")));
    QVERIFY(reported.contains(QByteArrayLiteral("!CFG BEGIN\r")));
    QVERIFY(reported.contains(QByteArrayLiteral("!CFG CHECK\r")));
    QVERIFY(std::any_of(reported.cbegin(), reported.cend(), [](const QByteArray &bytes) {
        return bytes.startsWith("!CFG APPLY ");
    }));
    for (const auto &bytes : reported) {
        QVERIFY(!bytes.contains("test-password"));
        QVERIFY(!bytes.contains("wifi-secret"));
    }
    const QStringList patches = m_commands.filter(QStringLiteral("!NET SET "));
    QCOMPARE(patches.size(), 1);
    QVERIFY(patches.first().contains(QStringLiteral("wifi-secret")));
    const QStringList applies = m_commands.filter(QStringLiteral("!CFG APPLY "));
    QCOMPARE(applies.size(), 1);
    const QByteArray payload = applies.first().toUtf8().mid(QByteArrayLiteral("!CFG APPLY ").size());
    const QJsonObject applied = QJsonDocument::fromJson(payload).object();
    QCOMPARE(applied.value(QStringLiteral("candidate_id")).toDouble(), double(candidate));
    QCOMPARE(applied.value(QStringLiteral("revision")).toDouble(), double(revision));
    m_connection->disconnectRequested();
}

void ApProvisioningConnectionTests::wifiRejectsInvalidConfigurationIds_data()
{
    QTest::addColumn<QJsonValue>("id");
    QTest::newRow("null") << QJsonValue();
    QTest::newRow("string") << QJsonValue(QStringLiteral("7"));
    QTest::newRow("fractional") << QJsonValue(7.5);
    QTest::newRow("negative") << QJsonValue(-1);
    QTest::newRow("imprecise") << QJsonValue(9007199254740992.0);
}

void ApProvisioningConnectionTests::wifiRejectsInvalidConfigurationIds()
{
    QFETCH(QJsonValue, id);
    QVERIFY(connectDevice());
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    QSignalSpy sent(m_transport, &Communication::Transport::bytesSent);
    connect(m_peer, &QWebSocket::textMessageReceived, this, [this, id](const QString &message) {
        QString command;
        QJsonObject data;
        if (message.startsWith(QStringLiteral("!SESSION AUTH "))) {
            command = QStringLiteral("SESSION AUTH");
        } else if (message == QStringLiteral("!CFG BEGIN\r")) {
            command = QStringLiteral("CFG BEGIN");
            data = {{"candidate_id", id}, {"base_revision", 2}};
        } else {
            return;
        }
        const QJsonObject response{{"cmd", command}, {"ok", true}, {"data", data}};
        m_peer->sendTextMessage(QLatin1Char('!') +
                               QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)) +
                               QStringLiteral("\r\n"));
    });

    m_window->findChild<QAction *>(QStringLiteral("wifiAction"))->trigger();
    submitPassword();
    QTRY_VERIFY(m_window->findChild<WifiProvisioningDialog *>());
    m_window->findChild<WifiProvisioningDialog *>()->configurationRequested(
        QStringLiteral("test-wifi"), QStringLiteral("wifi-secret"));
    QTRY_VERIFY(std::any_of(received.cbegin(), received.cend(), [](const QList<QVariant> &arguments) {
        return arguments.first().toByteArray().contains("\"cmd\":\"CFG BEGIN\"");
    }));
    QVERIFY(std::none_of(sent.cbegin(), sent.cend(), [](const QList<QVariant> &arguments) {
        const QByteArray command = arguments.first().toByteArray();
        return command.startsWith("!NET SET ") || command.startsWith("!CFG APPLY ");
    }));
}

void ApProvisioningConnectionTests::ipQueryDoesNotRequireAuthentication()
{
    QVERIFY(connectDevice());
    auto *query = m_window->findChild<QAction *>(QStringLiteral("getIpAddressesAction"));
    QVERIFY(query);
    query->trigger();
    QTRY_COMPARE(m_commands.filter(QStringLiteral("!NET GET")),
                 QStringList{QStringLiteral("!NET GET\r")});
    QVERIFY(m_commands.filter(QStringLiteral("!SESSION AUTH")).isEmpty());
    QVERIFY(!m_window->findChild<ManagementKeyDialog *>());
    QVERIFY(m_commands.filter(QStringLiteral("!PROV STATUS")).isEmpty());

    m_peer->sendTextMessage(QStringLiteral(
        "!{\"cmd\":\"NET GET\",\"ok\":true,\"data\":{\"runtime\":null}}\r\n"));
    QTRY_COMPARE(m_commands.filter(QStringLiteral("!PROV STATUS")),
                 QStringList{QStringLiteral("!PROV STATUS\r")});
    QVERIFY(m_commands.filter(QStringLiteral("!SESSION AUTH")).isEmpty());
}

void ApProvisioningConnectionTests::ipQueryPreservesWifiTrial()
{
    QVERIFY(connectDevice());
    connect(m_peer, &QWebSocket::textMessageReceived, this, [this](const QString &message) {
        QString command;
        QJsonObject data;
        if (message.startsWith(QStringLiteral("!SESSION AUTH "))) {
            command = QStringLiteral("SESSION AUTH");
        } else if (message == QStringLiteral("!CFG BEGIN\r")) {
            command = QStringLiteral("CFG BEGIN");
            data = {{"candidate_id", 7}, {"base_revision", 2}};
        } else if (message.startsWith(QStringLiteral("!NET SET "))) {
            command = QStringLiteral("NET SET");
        } else if (message == QStringLiteral("!CFG CHECK\r")) {
            command = QStringLiteral("CFG CHECK");
        } else if (message.startsWith(QStringLiteral("!CFG APPLY "))) {
            command = QStringLiteral("CFG APPLY");
            data = {{"trial_id", 11}};
        } else {
            return;
        }
        const QJsonObject response{{"cmd", command}, {"ok", true}, {"data", data}};
        m_peer->sendTextMessage(QLatin1Char('!') +
                               QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)) +
                               QStringLiteral("\r\n"));
    });

    m_window->findChild<QAction *>(QStringLiteral("wifiAction"))->trigger();
    submitPassword();
    QTRY_VERIFY(m_window->findChild<WifiProvisioningDialog *>());
    m_window->findChild<WifiProvisioningDialog *>()->configurationRequested(
        QStringLiteral("test-wifi"), QStringLiteral("wifi-secret"));
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!NET GET\r")));

    m_window->findChild<QAction *>(QStringLiteral("getIpAddressesAction"))->trigger();
    QTRY_VERIFY(m_commands.count(QStringLiteral("!NET GET\r")) >= 2);
    QTimer confirmation;
    connect(&confirmation, &QTimer::timeout, this, [this] {
        for (auto *dialog : m_window->findChildren<QMessageBox *>()) {
            if (dialog->standardButtons().testFlag(QMessageBox::No))
                dialog->done(QMessageBox::No);
        }
    });
    confirmation.start(10);
    const QJsonObject data{{"runtime", QJsonObject{{"wifi", QJsonObject{
        {"connected", true}, {"ipv4_valid", true}, {"ssid", "test-wifi"},
        {"address", "192.0.2.20"}}}}}};
    const QJsonObject response{{"cmd", "NET GET"}, {"ok", true}, {"data", data}};
    m_peer->sendTextMessage(QLatin1Char('!') +
                           QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)) +
                           QStringLiteral("\r\n"));
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!PROV STATUS\r")));
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!CFG ABORT {\"trial_id\":11}\r")));
}

void ApProvisioningConnectionTests::failedAuthenticationDoesNotStartProvisioning()
{
    QVERIFY(connectDevice());
    m_start->trigger();
    submitPassword();
    QTRY_COMPARE(m_commands.filter(QStringLiteral("!SESSION AUTH")).size(), 1);

    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(QStringLiteral(
        "!{\"cmd\":\"SESSION AUTH\",\"ok\":false,\"error\":\"NOT_AUTHORIZED\"}\r\n"));
    QTRY_COMPARE(received.size(), 1);
    QVERIFY(m_commands.filter(QStringLiteral("!PROV START")).isEmpty());

    m_start->trigger();
    submitPassword();
    QTRY_COMPARE(m_commands.filter(QStringLiteral("!SESSION AUTH")).size(), 2);
    QVERIFY(m_commands.filter(QStringLiteral("!PROV START")).isEmpty());
}

void ApProvisioningConnectionTests::cancelledAuthenticationDoesNotStartProvisioning()
{
    QVERIFY(connectDevice());
    m_start->trigger();
    auto *dialog = m_window->findChild<ManagementKeyDialog *>();
    QVERIFY(dialog);
    dialog->reject();
    QCoreApplication::processEvents();
    QVERIFY(m_commands.filter(QStringLiteral("!SESSION AUTH")).isEmpty());
    QVERIFY(m_commands.filter(QStringLiteral("!PROV START")).isEmpty());
}

#ifdef Q_OS_UNIX
void ApProvisioningConnectionTests::serialStartUsesBareCommand()
{
    QSignalSpy sent(m_transport, &Communication::Transport::bytesSent);
    const int masterFd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    QVERIFY(masterFd >= 0);
    QFile master;
    QVERIFY(master.open(masterFd, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle));
    QVERIFY(::grantpt(masterFd) == 0);
    QVERIFY(::unlockpt(masterFd) == 0);
    const char *slave = ::ptsname(masterFd);
    QVERIFY(slave);
    m_window->findChild<QRadioButton *>(QStringLiteral("serialMode"))->setChecked(true);
    m_window->findChild<QComboBox *>(QStringLiteral("serialPort"))
        ->setEditText(QString::fromLocal8Bit(slave));
    m_connection->connectRequested();
    QVERIFY(m_connection->isConnected());
    QVERIFY(m_start->isEnabled());
    m_start->trigger();

    QByteArray commands;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        char buffer[1024];
        const auto count = ::read(masterFd, buffer, sizeof(buffer));
        if (count > 0)
            commands.append(buffer, count);
        return commands.contains("!PROV START") && commands.endsWith('\r');
    }(), 3000);
    QList<QByteArray> provisioningCommands;
    for (const QByteArray &command : commands.split('\r')) {
        if (command.startsWith("!PROV START"))
            provisioningCommands.append(command);
    }
    QCOMPARE(provisioningCommands, QList<QByteArray>{"!PROV START"});
    QVERIFY(!commands.contains("!SESSION AUTH"));
    QVERIFY(std::any_of(sent.cbegin(), sent.cend(), [](const QList<QVariant> &arguments) {
        return arguments.first().toByteArray() == QByteArrayLiteral("!PROV START\r");
    }));
}
#endif

QTEST_MAIN(ApProvisioningConnectionTests)
#include "ApProvisioningConnectionTests.moc"
