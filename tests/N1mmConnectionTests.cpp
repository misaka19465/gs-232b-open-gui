/*
 * GS-232B Open Control - rotator controller GUI.
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
#include "CalibrationDialog.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "MainWindow.h"
#include "n1mm/N1mmController.h"
#include "n1mm/N1mmNameDialog.h"
#include "n1mm/N1mmProtocol.h"

#include <QAction>
#include <QCheckBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QMap>
#include <QNetworkDatagram>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QUdpSocket>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest>

#include <utility>

namespace {

QByteArray turn(const QByteArray &name = "GS232B")
{
    return "<N1MMRotor><rotor>" + name +
           "</rotor><goazi>10.0</goazi><offset>0</offset><bidirectional>0</bidirectional><freqband>14</freqband></N1MMRotor>";
}

QByteArray stop(const QByteArray &name = "GS232B")
{
    return "<N1MMRotor><stop><rotor>" + name + "</rotor></stop></N1MMRotor>";
}

} // namespace

class N1mmConnectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void forwardsCommandsAndReportsOnlyMeasurements();
    void rejectsCommandsBeforeFeedbackAndAfterReconnect();
    void appliesCompensationAndRenamesImmediately();
    void pausesDuringCalibration_data();
    void pausesDuringCalibration();
    void stopsWithoutSendingPendingSliderCommand();
    void ignoresVirtualComMode();
    void restoresEnabledPreference();
    void manualModeReportsPositionAndDropsControl();
    void longPathDoesNotChangeN1mmTargets();

private:
    bool connectDevice();
    void send(const QByteArray &packet);
    QByteArray receivePosition();
    void enable();

    QTemporaryDir m_directory;
    MainWindow *m_window = nullptr;
    ConnectionDialog *m_connection = nullptr;
    N1mmController *m_controller = nullptr;
    QWebSocketServer *m_server = nullptr;
    QPointer<QWebSocket> m_peer;
    QUdpSocket m_sender;
    QUdpSocket m_receiver;
    QStringList m_commands;
    const QStringList m_preferenceKeys{QStringLiteral("n1mm/enabled"), QStringLiteral("n1mm/rotorName"),
                                      QStringLiteral("rotor/azimuthOffset"), QStringLiteral("rotor/shortestPathEnabled")};
    QMap<QString, QVariant> m_savedPreferences;
};

void N1mmConnectionTests::initTestCase()
{
    QVERIFY(m_directory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_directory.path());
}

void N1mmConnectionTests::init()
{
    QSettings raw(QSettings::NativeFormat, QSettings::UserScope,
                  QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"));
    m_savedPreferences.clear();
    for (const QString &key : m_preferenceKeys) {
        if (raw.contains(key))
            m_savedPreferences.insert(key, raw.value(key));
    }
    QVERIFY(AppSettings().setN1mmEnabled(false));
    QVERIFY(AppSettings().setN1mmRotorName(QStringLiteral("GS232B")));
    QVERIFY(AppSettings().setAzimuthOffset(0));
    QVERIFY(AppSettings().setShortestPathEnabled(true));
    m_commands.clear();
    QVERIFY(m_receiver.bind(QHostAddress::LocalHost, N1mm::ReportingPort, QUdpSocket::DontShareAddress));
    m_server = new QWebSocketServer(QStringLiteral("n1mm-test"), QWebSocketServer::NonSecureMode);
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    connect(m_server, &QWebSocketServer::newConnection, this, [this] {
        m_peer = m_server->nextPendingConnection();
        m_peer->setParent(m_server);
        connect(m_peer, &QWebSocket::textMessageReceived, this,
                [this](const QString &message) { m_commands.append(message); });
    });
    m_window = new MainWindow;
    m_connection = m_window->findChild<ConnectionDialog *>();
    m_controller = m_window->findChild<N1mmController *>();
    QVERIFY(m_connection);
    QVERIFY(m_controller);
    m_window->findChild<QRadioButton *>(QStringLiteral("networkMode"))->setChecked(true);
    m_window->findChild<QLineEdit *>(QStringLiteral("websocketAddress"))
        ->setText(QStringLiteral("ws://127.0.0.1:%1").arg(m_server->serverPort()));
}

void N1mmConnectionTests::cleanup()
{
    delete m_window;
    m_window = nullptr;
    delete m_server;
    m_server = nullptr;
    m_sender.close();
    m_receiver.close();
    QSettings raw(QSettings::NativeFormat, QSettings::UserScope,
                  QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"));
    for (const QString &key : m_preferenceKeys) {
        if (m_savedPreferences.contains(key))
            raw.setValue(key, m_savedPreferences.value(key));
        else
            raw.remove(key);
    }
    raw.sync();
}

bool N1mmConnectionTests::connectDevice()
{
    m_connection->connectRequested();
    return QTest::qWaitFor([this] { return m_peer && m_connection->isConnected(); }, 3000);
}

void N1mmConnectionTests::send(const QByteArray &packet)
{
    QCOMPARE(m_sender.writeDatagram(packet, QHostAddress::LocalHost, N1mm::ControlPort), packet.size());
}

QByteArray N1mmConnectionTests::receivePosition()
{
    if (!QTest::qWaitFor([this] { return m_receiver.hasPendingDatagrams(); }, 1000))
        return {};
    return m_receiver.receiveDatagram().data();
}

void N1mmConnectionTests::enable()
{
    m_window->findChild<QRadioButton *>(QStringLiteral("trackingMode"))->setChecked(true);
    m_window->findChild<QAction *>(QStringLiteral("n1mmEnableAction"))->setChecked(true);
    QVERIFY(m_controller->isEnabled());
}

void N1mmConnectionTests::forwardsCommandsAndReportsOnlyMeasurements()
{
    enable();
    QVERIFY(connectDevice());
    QVERIFY(!m_receiver.hasPendingDatagrams());
    auto *transport = m_window->findChild<Communication::Transport *>();
    QSignalSpy received(transport, &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(QStringLiteral("AZ=35"));
    QTRY_COMPARE(received.size(), 1);
    QVERIFY(!m_receiver.hasPendingDatagrams());
    m_peer->sendTextMessage(QStringLiteral("0 EL=020\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@3500"));
    send(turn());
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M370\r")));
    QVERIFY(!m_receiver.hasPendingDatagrams());
    m_peer->sendTextMessage(QStringLiteral("AZ=370 EL=020\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@100"));
    send(stop());
    QTRY_VERIFY(m_commands.contains(QStringLiteral("A\r")));
    m_peer->sendTextMessage(QStringLiteral("EL=030\r\n"));
    QTRY_COMPARE(received.size(), 4);
    QVERIFY(!m_receiver.hasPendingDatagrams());
}

void N1mmConnectionTests::rejectsCommandsBeforeFeedbackAndAfterReconnect()
{
    enable();
    send(turn());
    QVERIFY(connectDevice());
    QSignalSpy errors(m_controller, &N1mmController::errorOccurred);
    send(turn());
    QTRY_COMPARE(errors.size(), 1);
    QVERIFY(!m_commands.contains(QStringLiteral("M010\r")));
    m_peer->sendTextMessage(QStringLiteral("AZ=350 EL=000\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@3500"));
    m_connection->disconnectRequested();
    QTRY_VERIFY(!m_connection->isConnected());
    QVERIFY(!m_receiver.hasPendingDatagrams());
    send(turn());
    QVERIFY(connectDevice());
    send(turn());
    QTRY_COMPARE(errors.size(), 2);
    QVERIFY(!m_commands.contains(QStringLiteral("M370\r")));
    m_peer->sendTextMessage(QStringLiteral("AZ=350\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@3500"));
    send(turn());
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M370\r")));
}

void N1mmConnectionTests::appliesCompensationAndRenamesImmediately()
{
    enable();
    QVERIFY(connectDevice());
    m_controller->setAzimuthOffset(30);
    m_peer->sendTextMessage(QStringLiteral("AZ=350 EL=020\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@200"));
    m_window->findChild<QAction *>(QStringLiteral("n1mmNameAction"))->trigger();
    auto *dialog = m_window->findChild<N1mmNameDialog *>();
    QVERIFY(dialog);
    dialog->findChild<QLineEdit *>(QStringLiteral("n1mmRotorName"))->setText(QStringLiteral("Tower A"));
    dialog->findChild<QPushButton *>(QStringLiteral("saveN1mmRotorName"))->click();
    QCOMPARE(AppSettings().n1mmRotorName(), QStringLiteral("Tower A"));
    QCOMPARE(receivePosition(), QByteArray("Tower A@200"));
    send(turn());
    send(turn("Tower A"));
    send(stop("Tower A"));
    QTRY_VERIFY(m_commands.contains(QStringLiteral("A\r")));
    QCOMPARE(m_commands.count(QStringLiteral("M340\r")), 1);
    QVERIFY(!m_commands.contains(QStringLiteral("M370\r")));
}

void N1mmConnectionTests::pausesDuringCalibration_data()
{
    QTest::addColumn<QString>("buttonName");
    QTest::addColumn<QString>("command");
    QTest::newRow("azimuth") << QStringLiteral("azimuthZeroCalibration") << QStringLiteral("O\r");
    QTest::newRow("elevation") << QStringLiteral("elevationZeroCalibration") << QStringLiteral("O2\r");
}

void N1mmConnectionTests::pausesDuringCalibration()
{
    QFETCH(QString, buttonName);
    QFETCH(QString, command);
    enable();
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(QStringLiteral("AZ=350 EL=000\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@3500"));
    m_window->findChild<QAction *>(QStringLiteral("calibrationAction"))->trigger();
    auto *dialog = m_window->findChild<CalibrationDialog *>();
    QVERIFY(dialog);
    auto *button = dialog->findChild<QPushButton *>(buttonName);
    QVERIFY(button && button->isEnabled());
    QTimer::singleShot(0, this, [this] {
        send(turn());
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *message = qobject_cast<QMessageBox *>(widget)) {
                QTimer::singleShot(30, message, [message] {
                    message->button(QMessageBox::Ok)->click();
                });
            }
        }
    });
    button->click();
    QTRY_VERIFY(m_commands.contains(command));
    QVERIFY(!button->isEnabled());
    QVERIFY(!m_window->findChild<QToolButton *>(QStringLiteral("azimuthClockwise"))->isEnabled());
    send(turn());
    send(stop());
    QTest::qWait(30);
    QVERIFY(!m_commands.contains(QStringLiteral("M370\r")));
    QVERIFY(!m_commands.contains(QStringLiteral("A\r")));
    m_peer->sendTextMessage(QStringLiteral("Completed\r\n"));
    auto *transport = m_window->findChild<Communication::Transport *>();
    QSignalSpy received(transport, &Communication::Transport::bytesReceived);
    QTRY_COMPARE(received.size(), 1);
    QVERIFY(button->isEnabled());
    QVERIFY(m_window->findChild<QToolButton *>(QStringLiteral("azimuthClockwise"))->isEnabled());
    QSignalSpy errors(m_controller, &N1mmController::errorOccurred);
    send(turn());
    QTRY_COMPARE(errors.size(), 1);
    m_peer->sendTextMessage(QStringLiteral("AZ=350\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@3500"));
    send(turn());
    QTRY_COMPARE(m_commands.count(QStringLiteral("M370\r")), 1);
}

void N1mmConnectionTests::stopsWithoutSendingPendingSliderCommand()
{
    enable();
    QVERIFY(connectDevice());
    m_window->findChild<QSlider *>(QStringLiteral("elevationSlider"))->setValue(45);
    send(stop());
    QTRY_VERIFY(m_commands.contains(QStringLiteral("A\r")));
    QTest::qWait(200);
    for (const QString &command : std::as_const(m_commands))
        QVERIFY(!command.startsWith(QLatin1Char('W')));
}

void N1mmConnectionTests::ignoresVirtualComMode()
{
    enable();
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(QStringLiteral("AZ=350\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@3500"));
    m_window->findChild<QCheckBox *>(QStringLiteral("virtualCom"))->setChecked(true);
    send(turn());
    send(stop());
    QTest::qWait(30);
    QVERIFY(!m_commands.contains(QStringLiteral("M370\r")));
    QVERIFY(!m_commands.contains(QStringLiteral("A\r")));
}

void N1mmConnectionTests::restoresEnabledPreference()
{
    enable();
    QVERIFY(AppSettings().n1mmEnabled());
    QVERIFY(AppSettings().setN1mmRotorName(QStringLiteral("Saved Tower")));
    delete m_window;
    m_window = new MainWindow;
    m_controller = m_window->findChild<N1mmController *>();
    QVERIFY(m_controller->isEnabled());
    QCOMPARE(m_controller->rotorName(), QStringLiteral("Saved Tower"));
    m_window->findChild<QAction *>(QStringLiteral("n1mmEnableAction"))->setChecked(false);
    QVERIFY(!m_controller->isEnabled());
    QVERIFY(!AppSettings().n1mmEnabled());
}

void N1mmConnectionTests::manualModeReportsPositionAndDropsControl()
{
    auto *manual = m_window->findChild<QRadioButton *>(QStringLiteral("manualMode"));
    auto *tracking = m_window->findChild<QRadioButton *>(QStringLiteral("trackingMode"));
    QVERIFY(manual->isChecked());
    m_window->findChild<QAction *>(QStringLiteral("n1mmEnableAction"))->setChecked(true);
    QVERIFY(connectDevice());
    QSignalSpy commands(m_controller, &N1mmController::commandRequested);
    QSignalSpy errors(m_controller, &N1mmController::errorOccurred);
    send(turn());
    send(stop());
    QTest::qWait(30);
    QVERIFY(commands.isEmpty());
    QVERIFY(errors.isEmpty());
    m_peer->sendTextMessage(QStringLiteral("AZ=350 EL=020\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@3500"));
    send(turn());
    send(stop());
    tracking->setChecked(true);
    QTest::qWait(30);
    QVERIFY(commands.isEmpty());
    send(turn());
    send(stop());
    QTRY_COMPARE(commands.size(), 2);
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M370\r")));
    QTRY_VERIFY(m_commands.contains(QStringLiteral("A\r")));
    manual->setChecked(true);
    send(turn());
    send(stop());
    m_peer->sendTextMessage(QStringLiteral("AZ=370\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@100"));
    QTest::qWait(30);
    QCOMPARE(commands.size(), 2);
}

void N1mmConnectionTests::longPathDoesNotChangeN1mmTargets()
{
    enable();
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(QStringLiteral("AZ=350 EL=020\r\n"));
    QCOMPARE(receivePosition(), QByteArray("GS232B@3500"));
    auto *longPath = m_window->findChild<QCheckBox *>(QStringLiteral("longPath"));
    longPath->setChecked(true);
    send(turn());
    QTRY_COMPARE(m_commands.count(QStringLiteral("M370\r")), 1);
    send(turn().replace("<goazi>10.0", "<goazi>190.0"));
    QTRY_COMPARE(m_commands.count(QStringLiteral("M190\r")), 1);
    longPath->setChecked(false);
    send(turn());
    QTRY_COMPARE(m_commands.count(QStringLiteral("M370\r")), 2);
    QVERIFY(!m_receiver.hasPendingDatagrams());
}

QTEST_MAIN(N1mmConnectionTests)
#include "N1mmConnectionTests.moc"
