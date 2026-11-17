#include "AppSettings.h"
#include "AzimuthOffsetDialog.h"
#include "CalibrationDialog.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "MainWindow.h"
#include "gridtracker/GridTrackerController.h"
#include "gridtracker/GridTrackerProtocol.h"
#include "n1mm/N1mmController.h"
#include "n1mm/N1mmProtocol.h"

#include <QAction>
#include <QCheckBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkDatagram>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest>

namespace {

const QByteArray Turn = "<PST><AZIMUTH>10</AZIMUTH></PST>";
const QByteArray Poll = "<PST>AZ?</PST><PST>EL?</PST>";

} // namespace

class GridTrackerConnectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void forwardsMotionAndReportsOnlyMeasurements();
    void rejectsCommandsUntilFeedbackAfterReconnect();
    void updatesCompensationAndPathPreference();
    void pausesDuringCalibration_data();
    void pausesDuringCalibration();
    void ignoresVirtualSerialMode();
    void restoresPreferenceAndCoexistsWithN1mm();
    void keepsDisabledOnPortConflict();
    void manualModeAnswersQueriesAndDropsControl();
    void longPathOnlyChangesRequestedAzimuth();

private:
    bool connectDevice();
    void enable();
    void send(const QByteArray &packet);
    void feed(const QString &text);
    QByteArray receive();

    QTemporaryDir m_directory;
    MainWindow *m_window = nullptr;
    ConnectionDialog *m_connection = nullptr;
    GridTrackerController *m_controller = nullptr;
    QWebSocketServer *m_server = nullptr;
    QPointer<QWebSocket> m_peer;
    QUdpSocket m_sender;
    QUdpSocket m_receiver;
    QStringList m_commands;
};

void GridTrackerConnectionTests::initTestCase()
{
    QVERIFY(m_directory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_directory.path());
}

void GridTrackerConnectionTests::init()
{
    QSettings raw(QSettings::NativeFormat, QSettings::UserScope,
                  QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"));
    raw.clear();
    raw.sync();
    m_commands.clear();
    QVERIFY(m_receiver.bind(QHostAddress::LocalHost, GridTracker::ReportingPort, QUdpSocket::DontShareAddress));
    m_server = new QWebSocketServer(QStringLiteral("gridtracker-test"), QWebSocketServer::NonSecureMode);
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    connect(m_server, &QWebSocketServer::newConnection, this, [this] {
        m_peer = m_server->nextPendingConnection();
        m_peer->setParent(m_server);
        connect(m_peer, &QWebSocket::textMessageReceived, this,
                [this](const QString &message) { m_commands.append(message); });
    });
    m_window = new MainWindow;
    m_connection = m_window->findChild<ConnectionDialog *>();
    m_controller = m_window->findChild<GridTrackerController *>();
    QVERIFY(m_connection && m_controller);
    m_window->findChild<QRadioButton *>(QStringLiteral("networkMode"))->setChecked(true);
    m_window->findChild<QLineEdit *>(QStringLiteral("websocketAddress"))
        ->setText(QStringLiteral("ws://127.0.0.1:%1").arg(m_server->serverPort()));
}

void GridTrackerConnectionTests::cleanup()
{
    delete m_window;
    m_window = nullptr;
    delete m_server;
    m_server = nullptr;
    m_sender.close();
    m_receiver.close();
}

bool GridTrackerConnectionTests::connectDevice()
{
    m_connection->connectRequested();
    return QTest::qWaitFor([this] { return m_peer && m_connection->isConnected(); }, 3000);
}

void GridTrackerConnectionTests::enable()
{
    m_window->findChild<QRadioButton *>(QStringLiteral("trackingMode"))->setChecked(true);
    m_window->findChild<QAction *>(QStringLiteral("gridTrackerEnableAction"))->setChecked(true);
    QVERIFY(m_controller->isEnabled());
}

void GridTrackerConnectionTests::send(const QByteArray &packet)
{
    QCOMPARE(m_sender.writeDatagram(packet, QHostAddress::LocalHost, GridTracker::ControlPort), packet.size());
}

void GridTrackerConnectionTests::feed(const QString &text)
{
    QSignalSpy received(m_window->findChild<Communication::Transport *>(), &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(text);
    QTRY_COMPARE(received.size(), 1);
}

QByteArray GridTrackerConnectionTests::receive()
{
    if (!QTest::qWaitFor([this] { return m_receiver.hasPendingDatagrams(); }, 1000))
        return {};
    return m_receiver.receiveDatagram().data();
}

void GridTrackerConnectionTests::forwardsMotionAndReportsOnlyMeasurements()
{
    enable();
    QVERIFY(connectDevice());
    feed(QStringLiteral("AZ=35"));
    send(Poll);
    QSignalSpy errors(m_controller, &GridTrackerController::errorOccurred);
    send(Turn);
    QTRY_COMPARE(errors.size(), 1);
    QVERIFY(!m_receiver.hasPendingDatagrams());
    feed(QStringLiteral("0 EL=020\r\n"));
    send(Poll);
    QCOMPARE(receive(), QByteArray("AZ:350"));
    QCOMPARE(receive(), QByteArray("EL:20"));

    m_window->findChild<QSlider *>(QStringLiteral("elevationSlider"))->setValue(45);
    send(Turn);
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M370\r")));
    QTest::qWait(200);
    QVERIFY(!m_commands.contains(QStringLiteral("W350 045\r")));
    send(Poll);
    QCOMPARE(receive(), QByteArray("AZ:350"));
    QCOMPARE(receive(), QByteArray("EL:20"));
    send("<PST><AZIMUTH>10</AZIMUTH><ELEVATION>45</ELEVATION></PST>");
    QTRY_VERIFY(m_commands.contains(QStringLiteral("W370 045\r")));
    feed(QStringLiteral("AZ=370 EL=045\r\n"));
    send(Poll);
    QCOMPARE(receive(), QByteArray("AZ:10"));
    QCOMPARE(receive(), QByteArray("EL:45"));
    feed(QStringLiteral("EL=030\r\n"));
    send(Poll);
    QCOMPARE(receive(), QByteArray("AZ:10"));
    QCOMPARE(receive(), QByteArray("EL:30"));
    send("<PST><ELEVATION>0</ELEVATION></PST>");
    QTRY_VERIFY(m_commands.contains(QStringLiteral("W370 000\r")));
}

void GridTrackerConnectionTests::rejectsCommandsUntilFeedbackAfterReconnect()
{
    enable();
    send(Turn);
    QVERIFY(connectDevice());
    feed(QStringLiteral("AZ=350 EL=000\r\n"));
    m_connection->disconnectRequested();
    QTRY_VERIFY(!m_connection->isConnected());
    send(Turn);
    QVERIFY(connectDevice());
    QSignalSpy errors(m_controller, &GridTrackerController::errorOccurred);
    send(Poll);
    send(Turn);
    QTRY_COMPARE(errors.size(), 1);
    QVERIFY(!m_receiver.hasPendingDatagrams());
    QVERIFY(!m_commands.contains(QStringLiteral("M370\r")));
    feed(QStringLiteral("EL=020\r\n"));
    send(Turn);
    QTRY_COMPARE(errors.size(), 2);
    feed(QStringLiteral("AZ=350\r\n"));
    send(Turn);
    QTRY_COMPARE(m_commands.count(QStringLiteral("M370\r")), 1);
}

void GridTrackerConnectionTests::updatesCompensationAndPathPreference()
{
    enable();
    QVERIFY(connectDevice());
    feed(QStringLiteral("AZ=350 EL=020\r\n"));
    auto *shortest = m_window->findChild<QAction *>(QStringLiteral("shortestPathAction"));
    shortest->setChecked(false);
    send(Turn);
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M010\r")));
    shortest->setChecked(true);
    m_window->findChild<QAction *>(QStringLiteral("azimuthOffsetAction"))->trigger();
    auto *dialog = m_window->findChild<AzimuthOffsetDialog *>();
    QVERIFY(dialog);
    QVERIFY(AppSettings().setAzimuthOffset(30));
    dialog->accept();
    send(Poll);
    QCOMPARE(receive(), QByteArray("AZ:20"));
    QCOMPARE(receive(), QByteArray("EL:20"));
    send(Turn);
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M340\r")));
}

void GridTrackerConnectionTests::pausesDuringCalibration_data()
{
    QTest::addColumn<bool>("confirm");
    QTest::newRow("confirmed") << true;
    QTest::newRow("cancelled") << false;
}

void GridTrackerConnectionTests::pausesDuringCalibration()
{
    QFETCH(bool, confirm);
    enable();
    QVERIFY(connectDevice());
    feed(QStringLiteral("AZ=350 EL=020\r\n"));
    m_window->findChild<QAction *>(QStringLiteral("calibrationAction"))->trigger();
    auto *dialog = m_window->findChild<CalibrationDialog *>();
    QVERIFY(dialog);
    QSignalSpy commands(m_controller, &GridTrackerController::commandRequested);
    QTimer::singleShot(0, this, [this, confirm] {
        send(Poll);
        send(Turn);
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *message = qobject_cast<QMessageBox *>(widget)) {
                QTimer::singleShot(30, message, [message, confirm] {
                    message->button(confirm ? QMessageBox::Ok : QMessageBox::Cancel)->click();
                });
            }
        }
    });
    dialog->findChild<QPushButton *>(QStringLiteral("azimuthZeroCalibration"))->click();
    QVERIFY(commands.isEmpty());
    QVERIFY(!m_receiver.hasPendingDatagrams());
    if (confirm) {
        QTRY_VERIFY(m_commands.contains(QStringLiteral("O\r")));
        send(Turn);
        send(Poll);
        QTest::qWait(30);
        QVERIFY(commands.isEmpty());
        QVERIFY(!m_receiver.hasPendingDatagrams());
        feed(QStringLiteral("Completed\r\n"));
    }
    QSignalSpy errors(m_controller, &GridTrackerController::errorOccurred);
    send(Turn);
    QTRY_COMPARE(errors.size(), 1);
    feed(QStringLiteral("AZ=350 EL=020\r\n"));
    send(Turn);
    QTRY_COMPARE(commands.size(), 1);
    QTRY_COMPARE(m_commands.count(QStringLiteral("M370\r")), 1);
}

void GridTrackerConnectionTests::ignoresVirtualSerialMode()
{
    enable();
    QVERIFY(connectDevice());
    feed(QStringLiteral("AZ=350 EL=020\r\n"));
    m_window->findChild<QCheckBox *>(QStringLiteral("virtualCom"))->setChecked(true);
    QSignalSpy commands(m_controller, &GridTrackerController::commandRequested);
    send(Turn);
    send(Poll);
    QTest::qWait(30);
    QVERIFY(commands.isEmpty());
    QVERIFY(!m_receiver.hasPendingDatagrams());
}

void GridTrackerConnectionTests::restoresPreferenceAndCoexistsWithN1mm()
{
    enable();
    QVERIFY(AppSettings().gridTrackerEnabled());
    delete m_window;
    m_window = new MainWindow;
    m_controller = m_window->findChild<GridTrackerController *>();
    QVERIFY(m_controller->isEnabled());
    auto *n1mm = m_window->findChild<N1mmController *>();
    m_window->findChild<QAction *>(QStringLiteral("n1mmEnableAction"))->setChecked(true);
    QVERIFY(n1mm->isEnabled());
    QVERIFY(m_controller->isEnabled());
    m_window->findChild<QAction *>(QStringLiteral("gridTrackerEnableAction"))->setChecked(false);
    QVERIFY(!m_controller->isEnabled());
    QVERIFY(!AppSettings().gridTrackerEnabled());
    QVERIFY(n1mm->isEnabled());
}

void GridTrackerConnectionTests::keepsDisabledOnPortConflict()
{
    QUdpSocket blocker;
    QVERIFY(blocker.bind(QHostAddress::LocalHost, GridTracker::ControlPort, QUdpSocket::DontShareAddress));
    QTimer::singleShot(0, this, [] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *message = qobject_cast<QMessageBox *>(widget))
                message->accept();
        }
    });
    m_window->findChild<QAction *>(QStringLiteral("gridTrackerEnableAction"))->setChecked(true);
    QVERIFY(!m_controller->isEnabled());
    QVERIFY(!AppSettings().gridTrackerEnabled());
    blocker.close();
    enable();
}

void GridTrackerConnectionTests::manualModeAnswersQueriesAndDropsControl()
{
    auto *manual = m_window->findChild<QRadioButton *>(QStringLiteral("manualMode"));
    auto *tracking = m_window->findChild<QRadioButton *>(QStringLiteral("trackingMode"));
    QVERIFY(manual->isChecked());
    m_window->findChild<QAction *>(QStringLiteral("gridTrackerEnableAction"))->setChecked(true);
    QVERIFY(connectDevice());
    QSignalSpy commands(m_controller, &GridTrackerController::commandRequested);
    QSignalSpy errors(m_controller, &GridTrackerController::errorOccurred);
    send(Turn);
    send(Poll);
    QTest::qWait(30);
    QVERIFY(commands.isEmpty());
    QVERIFY(errors.isEmpty());
    QVERIFY(!m_receiver.hasPendingDatagrams());
    feed(QStringLiteral("AZ=350 EL=020\r\n"));
    send(Turn);
    send("<PST><ELEVATION>45</ELEVATION></PST>");
    send(Poll);
    QCOMPARE(receive(), QByteArray("AZ:350"));
    QCOMPARE(receive(), QByteArray("EL:20"));
    QVERIFY(commands.isEmpty());
    send(Turn);
    send(Poll);
    tracking->setChecked(true);
    QCOMPARE(receive(), QByteArray("AZ:350"));
    QCOMPARE(receive(), QByteArray("EL:20"));
    QVERIFY(commands.isEmpty());
    send(Turn);
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M370\r")));
    manual->setChecked(true);
    send(Turn);
    send(Poll);
    QCOMPARE(receive(), QByteArray("AZ:350"));
    QCOMPARE(receive(), QByteArray("EL:20"));
    QCOMPARE(commands.size(), 1);
    QVERIFY(errors.isEmpty());
}

void GridTrackerConnectionTests::longPathOnlyChangesRequestedAzimuth()
{
    enable();
    QVERIFY(connectDevice());
    feed(QStringLiteral("AZ=350 EL=020\r\n"));
    auto *longPath = m_window->findChild<QCheckBox *>(QStringLiteral("longPath"));
    longPath->setChecked(true);
    send(Turn);
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M190\r")));
    send("<PST><AZIMUTH>190</AZIMUTH><ELEVATION>45</ELEVATION></PST>");
    QTRY_VERIFY(m_commands.contains(QStringLiteral("W370 045\r")));
    m_window->findChild<QAction *>(QStringLiteral("shortestPathAction"))->setChecked(false);
    send("<PST><AZIMUTH>190</AZIMUTH></PST>");
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M010\r")));
    m_controller->setAzimuthOffset(30);
    send(Turn);
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M160\r")));
    send("<PST><ELEVATION>0</ELEVATION></PST>");
    QTRY_VERIFY(m_commands.contains(QStringLiteral("W350 000\r")));
    send(Poll);
    QCOMPARE(receive(), QByteArray("AZ:20"));
    QCOMPARE(receive(), QByteArray("EL:20"));
    longPath->setChecked(false);
    send(Turn);
    QTRY_VERIFY(m_commands.contains(QStringLiteral("M340\r")));
}

QTEST_MAIN(GridTrackerConnectionTests)
#include "GridTrackerConnectionTests.moc"
