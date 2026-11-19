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

#include "AppSettings.h"
#include "CommunicationProtocol.h"
#include "ControlPanel.h"
#include "ConnectionDialog.h"
#include "MainWindow.h"
#include "RotorModelDialog.h"
#include "ManagementKeyDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSettings>
#include <QSlider>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest>

#include <utility>

#ifdef Q_OS_UNIX
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

QString rotorResponse(const QJsonValue &modelId, const QString &command = QStringLiteral("ROTOR GET"))
{
    const QJsonObject data{{"model_id", modelId}, {"active_model_id", 1},
                           {"restart_required", true}};
    const QJsonObject response{{"cmd", command}, {"ok", true}, {"data", data}};
    return QLatin1Char('!') + QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact))
           + QStringLiteral("\r\n");
}

} // namespace

class RotorModelConnectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void persistsAndRestoresConnectionPreferences();
    void restoresOnlyScannedSerialPorts();
    void readsConfiguredModel_data();
    void readsConfiguredModel();
    void invalidResponsesPreserveSelection();
    void reconnectClearsAndReadsAgain();
    void writesAfterAuthenticationAndAcknowledgement();
    void failedWritesPreserveSavedModel_data();
    void failedWritesPreserveSavedModel();
    void closingDuringAuthenticationCancelsWrite();
    void writeTimeoutReadsSavedModelAgain();
#ifdef Q_OS_UNIX
    void virtualComDoesNotQuery();
    void serialConnectionReadsModel();
#endif

private:
    bool connectDevice();
    void selectModel(int modelId);
    void submitPassword();

    QTemporaryDir m_settingsDirectory;
    MainWindow *m_window = nullptr;
    ConnectionDialog *m_connection = nullptr;
    RotorModelDialog *m_modelDialog = nullptr;
    Communication::Transport *m_transport = nullptr;
    QWebSocketServer *m_server = nullptr;
    QPointer<QWebSocket> m_peer;
    QStringList m_commands;
};

void RotorModelConnectionTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDirectory.path());
}

void RotorModelConnectionTests::init()
{
    m_server = new QWebSocketServer(QStringLiteral("rotor-test"), QWebSocketServer::NonSecureMode);
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    connect(m_server, &QWebSocketServer::newConnection, this, [this] {
        m_peer = m_server->nextPendingConnection();
        m_peer->setParent(m_server);
        connect(m_peer, &QWebSocket::textMessageReceived, this,
                [this](const QString &message) { m_commands.append(message); });
    });

    m_window = new MainWindow;
    m_connection = m_window->findChild<ConnectionDialog *>();
    m_modelDialog = m_window->findChild<RotorModelDialog *>();
    m_transport = m_window->findChild<Communication::Transport *>();
    QVERIFY(m_connection);
    QVERIFY(m_modelDialog);
    QVERIFY(m_transport);
    m_window->findChild<QRadioButton *>(QStringLiteral("networkMode"))->setChecked(true);
    m_window->findChild<QLineEdit *>(QStringLiteral("websocketAddress"))
        ->setText(QStringLiteral("ws://127.0.0.1:%1").arg(m_server->serverPort()));
}

void RotorModelConnectionTests::cleanup()
{
    delete m_window;
    m_window = nullptr;
    delete m_server;
    m_server = nullptr;
    m_commands.clear();
}

bool RotorModelConnectionTests::connectDevice()
{
    m_connection->connectRequested();
    return QTest::qWaitFor([this] { return m_peer && m_connection->isConnected(); }, 3000);
}

void RotorModelConnectionTests::selectModel(int modelId)
{
    m_window->findChild<QAction *>(QStringLiteral("rotorModelAction"))->trigger();
    auto *model = m_modelDialog->findChild<QComboBox *>(QStringLiteral("rotorModel"));
    model->setCurrentIndex(model->findData(modelId));
}

void RotorModelConnectionTests::submitPassword()
{
    auto *dialog = m_modelDialog->findChild<ManagementKeyDialog *>();
    QVERIFY(dialog);
    dialog->findChild<QLineEdit *>(QStringLiteral("managementPassword"))
        ->setText(QStringLiteral("test-password"));
    dialog->findChild<QPushButton *>(QStringLiteral("authorizeButton"))->click();
}

void RotorModelConnectionTests::persistsAndRestoresConnectionPreferences()
{
    ConnectionDialog panel;
    auto *serial = panel.findChild<QComboBox *>(QStringLiteral("serialPort"));
    const QString selectedPort = serial->itemData(serial->count() - 1).toString();
    if (!selectedPort.isEmpty()) {
        // NOTE: 连接操作也保存用户接受的默认串口，无需先切换到其他项。
        panel.findChild<QRadioButton *>(QStringLiteral("serialMode"))->setChecked(true);
        panel.connectRequested();
        QSettings raw(QSettings::NativeFormat, QSettings::UserScope,
                      QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"));
        QCOMPARE(raw.value(QStringLiteral("connection/serialPort")).toString(), panel.connectionOptions().serialPort);
        serial->setCurrentIndex(serial->count() - 1);
    }
    panel.findChild<QComboBox *>(QStringLiteral("baudRate"))->setCurrentText(QStringLiteral("2400"));
    panel.findChild<QLineEdit *>(QStringLiteral("websocketAddress"))->setText(QStringLiteral("192.0.2.10"));
    for (bool network : {true, false}) {
        panel.findChild<QRadioButton *>(network ? QStringLiteral("networkMode")
                                                : QStringLiteral("serialMode"))->setChecked(true);
        ConnectionDialog restored;
        const auto options = restored.connectionOptions();
        QCOMPARE(options.serialPort, selectedPort);
        QCOMPARE(options.baudRate, 2400);
        QCOMPARE(options.address, QStringLiteral("192.0.2.10"));
        QCOMPARE(restored.isNetworkMode(), network);
        QVERIFY(!restored.isConnected());
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("serialPort"))->isEnabled(), !network);
        QCOMPARE(AppSettings().baudRate(), 2400);
        QCOMPARE(AppSettings().connectionAddress(), QStringLiteral("192.0.2.10"));
        QCOMPARE(AppSettings().networkMode(), network);
    }
}

void RotorModelConnectionTests::restoresOnlyScannedSerialPorts()
{
    const QString missingPort = QStringLiteral("gs232b-missing-port");
    QVERIFY(AppSettings().setSerialPort(missingPort));
    {
        ConnectionDialog panel;
        auto *serial = panel.findChild<QComboBox *>(QStringLiteral("serialPort"));
        const QString firstPort = serial->itemData(0).toString();
        QCOMPARE(serial->currentIndex(), 0);
        QCOMPARE(panel.connectionOptions().serialPort, firstPort);
        panel.findChild<QComboBox *>(QStringLiteral("baudRate"))->setCurrentText(QStringLiteral("4800"));
        panel.findChild<QLineEdit *>(QStringLiteral("websocketAddress"))->setText(QStringLiteral("rotor.local"));
        panel.findChild<QRadioButton *>(QStringLiteral("serialMode"))->setChecked(true);
        panel.setConnecting(true);
        panel.setConnecting(false);
    }
    QSettings raw(QSettings::NativeFormat, QSettings::UserScope,
                  QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"));
    QCOMPARE(raw.value(QStringLiteral("connection/serialPort")).toString(), missingPort);
}

void RotorModelConnectionTests::readsConfiguredModel_data()
{
    QTest::addColumn<int>("modelId");
    for (int id = 1; id <= 16; ++id)
        QTest::newRow(qPrintable(QString::number(id))) << id;
}

void RotorModelConnectionTests::readsConfiguredModel()
{
    QFETCH(int, modelId);
    QCOMPARE(m_modelDialog->savedModelId(), 0);
    QVERIFY(connectDevice());
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!ROTOR GET\r")));
    QCOMPARE(m_commands.count(QStringLiteral("!ROTOR GET\r")), 1);
    QCOMPARE(m_commands.first(), QStringLiteral("!ROTOR GET\r"));
    for (const QString &command : std::as_const(m_commands)) {
        QVERIFY(!command.startsWith(QStringLiteral("!SESSION AUTH")));
        QVERIFY(!command.startsWith(QStringLiteral("!ROTOR SET")));
    }
    auto *apply = m_modelDialog->findChild<QPushButton *>(QStringLiteral("applyModel"));
    QVERIFY(!apply->isEnabled());
    QCOMPARE(m_modelDialog->savedModelId(), 0);

    const QString response = rotorResponse(modelId);
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(response.left(response.size() / 2));
    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(m_modelDialog->savedModelId(), 0);
    m_peer->sendTextMessage(response.mid(response.size() / 2));
    QTRY_COMPARE(m_modelDialog->savedModelId(), modelId);
    QVERIFY(!apply->isEnabled());

    // NOTE: 已保存型号决定可操作的轴，重启前的生效型号不得覆盖该选择。
    const bool elevationSupported = modelId <= 4 || modelId == 6 || modelId == 7 || modelId >= 15;
    auto *slider = m_window->findChild<QSlider *>(QStringLiteral("elevationSlider"));
    QCOMPARE(slider->isEnabled(), elevationSupported);
    QSignalSpy commands(m_window->findChild<ControlPanel *>(), &ControlPanel::commandRequested);
    m_window->findChild<QPushButton *>(QStringLiteral("elevationPreset0"))->click();
    QCOMPARE(commands.size(), elevationSupported ? 1 : 0);

    m_peer->sendTextMessage(rotorResponse(16, QStringLiteral("ROTOR SET")));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 16);
    QVERIFY(slider->isEnabled());
}

void RotorModelConnectionTests::invalidResponsesPreserveSelection()
{
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(rotorResponse(5));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 5);
    const QList<QJsonValue> invalidIds = {
        QJsonValue(), QJsonValue(QJsonValue::Undefined), "6", true, 0, -1, 17, 6.5, 4294967296.0
    };
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    for (const QJsonValue &id : invalidIds) {
        const auto count = received.size();
        m_peer->sendTextMessage(rotorResponse(id));
        QTRY_COMPARE(received.size(), count + 1);
        QCOMPARE(m_modelDialog->savedModelId(), 5);
    }
    const QStringList unrelatedOrFailed = {
        QStringLiteral("!{\"cmd\":\"ROTOR GET\",\"ok\":false,\"error\":\"BUSY\",\"data\":{\"model_id\":6}}\r\n"),
        QStringLiteral("!{\"cmd\":\"ROTOR SET\",\"ok\":false,\"error\":\"BUSY\",\"data\":{\"model_id\":6}}\r\n"),
        QStringLiteral("!{\"cmd\":\"SYS INFO\",\"ok\":true,\"data\":{\"model_id\":6}}\r\n"),
        QStringLiteral("!invalid-json\r\n")
    };
    for (const QString &response : unrelatedOrFailed) {
        const auto count = received.size();
        m_peer->sendTextMessage(response);
        QTRY_COMPARE(received.size(), count + 1);
        QCOMPARE(m_modelDialog->savedModelId(), 5);
    }
}

void RotorModelConnectionTests::reconnectClearsAndReadsAgain()
{
    QVERIFY(connectDevice());
    QTRY_COMPARE(m_commands.count(QStringLiteral("!ROTOR GET\r")), 1);
    m_peer->sendTextMessage(rotorResponse(7));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 7);
    m_peer->close();
    QTRY_VERIFY(!m_connection->isConnected());
    QCOMPARE(m_modelDialog->savedModelId(), 0);
    m_peer.clear();

    m_transport->bytesReceived(rotorResponse(5).toUtf8());
    QCOMPARE(m_modelDialog->savedModelId(), 0);
    QVERIFY(connectDevice());
    QTRY_COMPARE(m_commands.count(QStringLiteral("!ROTOR GET\r")), 2);
    QCOMPARE(m_modelDialog->savedModelId(), 0);
    m_peer->sendTextMessage(rotorResponse(12));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 12);
}

void RotorModelConnectionTests::writesAfterAuthenticationAndAcknowledgement()
{
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(rotorResponse(5));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 5);
    selectModel(6);
    QCOMPARE(m_modelDialog->selectedModelId(), 6);
    QCOMPARE(m_modelDialog->savedModelId(), 5);
    auto *apply = m_modelDialog->findChild<QPushButton *>(QStringLiteral("applyModel"));
    QVERIFY(apply->isEnabled());
    apply->click();
    QVERIFY(!apply->isEnabled());
    submitPassword();
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!SESSION AUTH {\"password\":\"test-password\"}\r")));
    const QString write = QStringLiteral("!ROTOR SET {\"model_id\":6}\r");
    QVERIFY(!m_commands.contains(write));
    m_peer->sendTextMessage(QStringLiteral("!{\"cmd\":\"SESSION AUTH\",\"ok\":true}\r\n"));
    QTRY_VERIFY(m_commands.contains(write));
    QCOMPARE(m_modelDialog->savedModelId(), 5);
    m_modelDialog->modelApplyRequested(6);
    m_peer->sendTextMessage(rotorResponse(6, QStringLiteral("ROTOR SET")));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 6);
    QCOMPARE(m_commands.count(write), 1);
    QVERIFY(!apply->isEnabled());

    selectModel(7);
    m_modelDialog->reject();
    m_window->findChild<QAction *>(QStringLiteral("rotorModelAction"))->trigger();
    QCOMPARE(m_modelDialog->selectedModelId(), 6);
    QVERIFY(!apply->isEnabled());
}

void RotorModelConnectionTests::failedWritesPreserveSavedModel_data()
{
    QTest::addColumn<QString>("error");
    for (const auto &error : {"BUSY", "NOT_AUTHORIZED", "IO"})
        QTest::newRow(error) << QString::fromLatin1(error);
}

void RotorModelConnectionTests::failedWritesPreserveSavedModel()
{
    QFETCH(QString, error);
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(rotorResponse(5));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 5);
    m_transport->bytesReceived("!{\"cmd\":\"SESSION AUTH\",\"ok\":true}\r\n");
    selectModel(6);
    auto *apply = m_modelDialog->findChild<QPushButton *>(QStringLiteral("applyModel"));
    apply->click();
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!ROTOR SET {\"model_id\":6}\r")));
    QVERIFY(!apply->isEnabled());
    const QJsonObject response{{"cmd", "ROTOR SET"}, {"ok", false}, {"error", error}};
    m_peer->sendTextMessage(QLatin1Char('!') +
                           QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)) +
                           QStringLiteral("\r\n"));
    QTRY_VERIFY(apply->isEnabled());
    QCOMPARE(m_modelDialog->savedModelId(), 5);
    QCOMPARE(m_modelDialog->selectedModelId(), 6);
}

void RotorModelConnectionTests::closingDuringAuthenticationCancelsWrite()
{
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(rotorResponse(5));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 5);
    selectModel(6);
    m_modelDialog->findChild<QPushButton *>(QStringLiteral("applyModel"))->click();
    submitPassword();
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!SESSION AUTH {\"password\":\"test-password\"}\r")));
    m_modelDialog->reject();
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(QStringLiteral("!{\"cmd\":\"SESSION AUTH\",\"ok\":true}\r\n"));
    QTRY_COMPARE(received.size(), 1);
    QVERIFY(!m_commands.contains(QStringLiteral("!ROTOR SET {\"model_id\":6}\r")));
    QCOMPARE(m_modelDialog->savedModelId(), 5);
}

void RotorModelConnectionTests::writeTimeoutReadsSavedModelAgain()
{
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(rotorResponse(5));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 5);
    m_transport->bytesReceived("!{\"cmd\":\"SESSION AUTH\",\"ok\":true}\r\n");
    selectModel(6);
    auto *apply = m_modelDialog->findChild<QPushButton *>(QStringLiteral("applyModel"));
    apply->click();
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!ROTOR SET {\"model_id\":6}\r")));
    const int readsBefore = m_commands.count(QStringLiteral("!ROTOR GET\r"));
    auto *timeout = m_window->findChild<QTimer *>(QStringLiteral("rotorModelTimeoutTimer"));
    QVERIFY(timeout->isActive());
    QVERIFY(QMetaObject::invokeMethod(timeout, "timeout", Qt::DirectConnection));
    QTRY_COMPARE(m_commands.count(QStringLiteral("!ROTOR GET\r")), readsBefore + 1);
    QCOMPARE(m_modelDialog->savedModelId(), 0);
    QVERIFY(!apply->isEnabled());
    m_peer->sendTextMessage(rotorResponse(6));
    QTRY_COMPARE(m_modelDialog->savedModelId(), 6);
    QVERIFY(!apply->isEnabled());
}

#ifdef Q_OS_UNIX
void RotorModelConnectionTests::virtualComDoesNotQuery()
{
    m_window->findChild<QCheckBox *>(QStringLiteral("virtualCom"))->setChecked(true);
    QVERIFY(connectDevice());
    QTest::qWait(100);
    QVERIFY(m_commands.isEmpty());
    m_peer->sendTextMessage(rotorResponse(5));
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(m_modelDialog->savedModelId(), 0);
}

void RotorModelConnectionTests::serialConnectionReadsModel()
{
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

    QByteArray commands;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        char buffer[1024];
        const auto count = ::read(masterFd, buffer, sizeof(buffer));
        if (count > 0)
            commands.append(buffer, count);
        return commands.contains("!ROTOR GET\r");
    }(), 3000);
    QCOMPARE(commands.count("!ROTOR GET\r"), 1);
    QVERIFY(!commands.contains("!ROTOR SET"));
    const QByteArray response = rotorResponse(9).toUtf8();
    QCOMPARE(::write(masterFd, response.constData(), response.size()), response.size());
    QTRY_COMPARE(m_modelDialog->savedModelId(), 9);
    m_connection->disconnectRequested();
    QCOMPARE(m_modelDialog->savedModelId(), 0);
}
#endif

QTEST_MAIN(RotorModelConnectionTests)
#include "RotorModelConnectionTests.moc"
