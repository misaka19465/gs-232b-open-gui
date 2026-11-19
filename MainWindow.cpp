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

#include "MainWindow.h"
#include "AntennaGainDialog.h"
#include "AppSettings.h"
#include "AzimuthOffsetDialog.h"
#include "CalibrationDialog.h"
#include "CommandLog.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "ConsolePanel.h"
#include "ControlPanel.h"
#include "FaultStatusMonitor.h"
#include "FaultStatusPanel.h"
#include "FirmwareUpgradeDialog.h"
#include "ManagementKeyDialog.h"
#include "NetworkPortConfigDialog.h"
#include "PasswordChangeDialog.h"
#include "StationGridDialog.h"
#include "WifiProvisioningDialog.h"
#include "weather/WindWeatherSettingsDialog.h"
#include "weather/WindProtectionController.h"
#include "virtual_serial/VirtualSerialBridge.h"
#include "n1mm/N1mmController.h"
#include "gridtracker/GridTrackerController.h"
#include "wsjtx/WsjtXController.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace {

qint64 configurationId(const QJsonValue &value)
{
    // NOTE: Qt 5 的 JSON 数字使用 double；拒绝小数和超出精确整数范围的 ID，避免确认错误配置。
    constexpr double maximumExactInteger = 9007199254740991.0;
    const double number = value.toDouble(-1);
    if (!std::isfinite(number) || number < 0 || number > maximumExactInteger ||
        std::trunc(number) != number)
        return -1;
    return static_cast<qint64>(number);
}

QString managementAuthCommand(const QString &password)
{
    QJsonObject request;
    request.insert(QStringLiteral("password"), password);
    return QStringLiteral("!SESSION AUTH %1")
        .arg(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_persistedWindowSize = AppSettings().windowSize();
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(tr("GS-232B Open Control"));
    resize(1280, 360);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);
    m_connection = new ConnectionDialog(this);
    m_control = new ControlPanel(central);
    layout->addWidget(m_control, 1);
    m_console = new ConsolePanel(central);
    // NOTE: 控制台保持固定高度；显隐时由视图菜单同步调整窗口高度。
    m_console->setFixedHeight(220);
    layout->addWidget(m_console);
    m_faultStatus = new FaultStatusMonitor(this);
    setCentralWidget(central);

    m_transport = new Communication::Transport(this);
    m_windProtection = new WindProtectionController(this);
    const AppSettings weatherSettings;
    m_windProtection->setWeatherConfiguration(weatherSettings.windWeatherProvider(),
                                              weatherSettings.qWeatherApiHost(),
                                              weatherSettings.qWeatherApiKey(),
                                              weatherSettings.windWeatherTermsAccepted());
    m_windProtection->setStationGrid(weatherSettings.stationGrid());
    m_windProtection->setDirectionOffset(m_control->windDirectionOffset());
    m_windProtection->setAzimuthOffset(weatherSettings.azimuthOffset());
    m_windProtection->setEnabled(m_control->windProtectionEnabled());
    m_positionPollTimer = new QTimer(this);
    m_positionPollTimer->setInterval(1000);
    m_wifiTrialPollTimer = new QTimer(this);
    m_wifiTrialPollTimer->setInterval(1000);
    m_calibrationTimeoutTimer = new QTimer(this);
    m_calibrationTimeoutTimer->setSingleShot(true);
    m_calibrationTimeoutTimer->setInterval(60000);

    m_connectionStatus = new QLabel(this);
    m_connectionStatus->setObjectName(QStringLiteral("connectionStatus"));
    m_transportStatus = new QLabel(this);
    m_transportStatus->setObjectName(QStringLiteral("transportStatus"));
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_connectionStatus, 1);
    statusBar()->addPermanentWidget(m_transportStatus);
    statusBar()->addPermanentWidget(new FaultStatusPanel(m_faultStatus, statusBar()));
    connect(m_control, &ControlPanel::mapError, this, [this](const QString &message) {
        statusBar()->showMessage(message, 15000);
    });
    connect(m_control, &ControlPanel::inputError, this, [this](const QString &message) {
        statusBar()->showMessage(message, 15000);
    });
    connect(m_control, &ControlPanel::windProtectionEnabledChanged, this, [this](bool enabled) {
        if (!AppSettings().setWindProtectionEnabled(enabled))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
        m_windProtection->setEnabled(enabled);
        updateBridgeState();
    });
    connect(m_control, &ControlPanel::windDirectionOffsetChanged, this, [this](int degrees) {
        if (!AppSettings().setWindDirectionOffset(degrees))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
        m_windProtection->setDirectionOffset(degrees);
    });
    connect(m_windProtection, &WindProtectionController::targetBearingRequested,
            m_control, &ControlPanel::requestAzimuthBearing);
    connect(m_windProtection, &WindProtectionController::windDirectionChanged,
            m_control, &ControlPanel::setWindDirection);
    connect(m_windProtection, &WindProtectionController::statusChanged, this,
            [this](const QString &message) {
                if (!message.isEmpty())
                    statusBar()->showMessage(message, 15000);
            });
    setupRotorModel();
    createMenus();
    connect(m_control, &ControlPanel::trackingEnabledChanged, this, &MainWindow::updateBridgeState);
    setupVirtualSerial();
    connect(m_connection, &ConnectionDialog::stateChanged,
            this, &MainWindow::updateConnectionState);
    connect(m_connection, &ConnectionDialog::connectRequested,
            this, &MainWindow::openTransport);
    connect(m_connection, &ConnectionDialog::disconnectRequested,
            this, &MainWindow::closeTransport);
    connect(m_connection, &QDialog::rejected, this, [this] {
        if (!m_connection->isConnecting())
            return;
        // NOTE: 用户取消握手时主动关闭传输，本次断开不重新弹出刚取消的窗口。
        m_connectionPromptSuppressed = true;
        closeTransport();
        m_connectionPromptSuppressed = false;
    });
    connect(m_transport, &Communication::Transport::connected, this, [this] {
        if (m_connection->isVirtualComEnabled() &&
            !m_virtualSerial->start(m_connection->virtualSerialOptions())) {
            closeTransport();
            return;
        }
        m_connection->setConnecting(false);
        if (!m_connection->isVirtualComEnabled())
            sendControlCommand(QStringLiteral("!ROTOR GET"));
        m_connection->setConnected(true);
        if (!m_connection->isVirtualComEnabled()) {
            sendControlCommand(QStringLiteral("C2"));
            m_positionPollTimer->start();
        }
    });
    connect(m_transport, &Communication::Transport::disconnected, this, [this] {
        m_virtualSerial->stop();
        m_connection->setConnecting(false);
        m_connection->setConnected(false);
        m_positionPollTimer->stop();
        m_wifiTrialPollTimer->stop();
        m_calibrationTimeoutTimer->stop();
        m_receiveBuffer.clear();
        m_consoleForwardedBytes = 0;
        m_pendingCalibrationCommand.clear();
        updateCalibrationState();
        m_faultStatus->setPaused(false);
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
        m_pendingNetworkPatch = {};
        m_pendingNetworkWifiTarget = true;
        m_pendingWifiSsid.clear();
        m_pendingManagementPassword.clear();
        m_pendingManagementResponseCommand.clear();
        m_managementAuthenticated = false;
        m_pendingManagementContinuation = {};
        m_pendingManagementResponseContinuation = {};
        if (!m_connectionPromptSuppressed)
            openConnectionDialog();
    });
    connect(m_transport, &Communication::Transport::bytesReceived,
            this, &MainWindow::handleReceivedBytes);
    connect(m_transport, &Communication::Transport::bytesSent,
            m_console, &ConsolePanel::appendSent);
    connect(m_transport, &Communication::Transport::errorOccurred, this,
            [this](const QString &error) {
                const QString message = tr("Transport error: %1").arg(error);
                m_console->appendError(message);
                m_connection->setConnecting(false);
                m_connection->setError(message);
            });
    connect(m_console, &ConsolePanel::commandRequested, this,
            [this](const QString &command) {
                sendControlCommand(command);
            });
    connect(m_control, &ControlPanel::commandRequested,
            this, [this](const QString &command) { sendControlCommand(command); });
    connect(m_faultStatus, &FaultStatusMonitor::commandRequested,
            this, [this](const QString &command) { sendControlCommand(command); });
    connect(m_positionPollTimer, &QTimer::timeout, this, [this] {
        sendControlCommand(QStringLiteral("C2"));
    });
    connect(m_wifiTrialPollTimer, &QTimer::timeout, this, [this] {
        if (m_networkConfigurationStage != NetworkConfigurationStage::AwaitRuntime ||
            m_pendingNetworkTrial < 0) {
            m_wifiTrialPollTimer->stop();
            return;
        }
        if (++m_wifiTrialPollCount > 300) {
            m_wifiTrialPollTimer->stop();
            m_console->appendError(tr("Network configuration trial timed out; the device will roll it back."));
            m_networkConfigurationStage = NetworkConfigurationStage::Idle;
            m_pendingNetworkPatch = {};
            m_pendingWifiSsid.clear();
            return;
        }
        sendControlCommand(QStringLiteral("!NET GET"));
    });
    connect(m_calibrationTimeoutTimer, &QTimer::timeout, this, [this] {
        m_pendingCalibrationCommand.clear();
        updateBridgeState();
        updateCalibrationState();
        m_faultStatus->setPaused(false);
        if (m_transport->isOpen())
            m_positionPollTimer->start();
    });
    updateConnectionState();
    const int consoleHeight = m_console->isHidden()
                                  ? 0
                                  : m_console->height() + centralWidget()->layout()->spacing();
    resize(m_persistedWindowSize.width(), m_persistedWindowSize.height() + consoleHeight);
    m_windowSizeRestored = true;
    connect(qApp, &QCoreApplication::aboutToQuit, this,
            [this] { saveWindowSize(false); });
}

MainWindow::~MainWindow()
{
    m_control->cancelPendingMotion();
    // NOTE: 先关闭两侧连接，避免 QObject 子对象析构时回调已销毁的界面。
    m_n1mm->setEnabled(false);
    m_gridTracker->setEnabled(false);
    m_wsjtX->setEnabled(false);
    const QSignalBlocker transportBlocker(m_transport);
    const QSignalBlocker virtualSerialBlocker(m_virtualSerial);
    m_transport->close();
    m_virtualSerial->stop();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (!m_windowSizeRestored || isMaximized() || isFullScreen())
        return;

    int widthWithoutElevation = event->size().width();
    if (m_control->elevationSupported()) {
        const QMargins margins = centralWidget()->layout()->contentsMargins();
        widthWithoutElevation -=
            (event->size().width() - margins.left() - margins.right()) / 2;
    }
    int heightWithoutConsole = event->size().height();
    if (!m_console->isHidden())
        heightWithoutConsole -= m_console->height() + centralWidget()->layout()->spacing();
    const QSize size(widthWithoutElevation, heightWithoutConsole);
    if (m_persistedWindowSize != size)
        m_windowSizeSaved = false;
    m_persistedWindowSize = size;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowSize(true);
    QMainWindow::closeEvent(event);
}

void MainWindow::saveWindowSize(bool warnOnFailure)
{
    if (m_windowSizeSaved)
        return;

    if (!isMaximized() && !isFullScreen()) {
        int widthWithoutElevation = width();
        if (m_control->elevationSupported())
            widthWithoutElevation -= m_control->width() / 2;
        int heightWithoutConsole = height();
        if (!m_console->isHidden())
            heightWithoutConsole -= m_console->height() + centralWidget()->layout()->spacing();
        m_persistedWindowSize = QSize(widthWithoutElevation, heightWithoutConsole);
    }
    m_windowSizeSaved = AppSettings().setWindowSize(m_persistedWindowSize);
    if (!m_windowSizeSaved && warnOnFailure)
        QMessageBox::warning(this, tr("Application settings"),
                             tr("Could not save application settings."));
}

void MainWindow::setupVirtualSerial()
{
    m_virtualSerial = new VirtualSerialBridge(this);
    connect(m_virtualSerial, &VirtualSerialBridge::started, this, [this](const QString &port) {
        m_console->appendNotice(tr("Virtual serial port ready: %1").arg(port));
    });
    connect(m_virtualSerial, &VirtualSerialBridge::webSocketLineReady, this,
            [this](const QByteArray &line) {
                // NOTE: 直接发送带 CR 的原始字节，不经过终端命令的 UTF-8 编码。
                if (!m_transport->isOpen())
                    return;
                const QByteArray logBytes = Communication::commandForLog(line);
                const bool reportAsSent = logBytes == line;
                if (!m_transport->sendBytes(line, reportAsSent))
                    closeTransport();
                else if (!reportAsSent)
                    m_console->appendSent(logBytes);
            });
    connect(m_virtualSerial, &VirtualSerialBridge::errorOccurred, this,
            [this](const QString &error) {
                const QString message = tr("Virtual serial error: %1").arg(error);
                m_console->appendError(message);
                m_connection->setError(message);
            });
    connect(m_virtualSerial, &VirtualSerialBridge::stopped, this, &MainWindow::closeTransport);
}

void MainWindow::createMenus()
{
    // NOTE: 菜单固定在窗口内，所有控件和弹窗均沿用系统 Qt Widgets 样式。
    menuBar()->setNativeMenuBar(false);
    m_connectionAction = menuBar()->addAction(tr("Connect"));
    m_connectionAction->setObjectName(QStringLiteral("connectionAction"));
    connect(m_connectionAction, &QAction::triggered, this, [this] {
        if (m_connection->isConnected())
            m_connection->disconnectRequested();
        else
            openConnectionDialog();
    });
    auto *files = menuBar()->addMenu(tr("Files"));
    files->addAction(tr("Exit"), this, &QWidget::close);
    createViewMenu();
    const AppSettings preferences;
    m_control->setAntennaGain(preferences.antennaGain());
    auto *settings = menuBar()->addMenu(tr("Settings"));
    auto *stationGridAction = settings->addAction(
        tr("My grid..."), this, &MainWindow::openStationGrid);
    stationGridAction->setObjectName(QStringLiteral("stationGridAction"));
    auto *offsetAction = settings->addAction(
        tr("Azimuth offset..."), this, &MainWindow::openAzimuthOffset);
    offsetAction->setObjectName(QStringLiteral("azimuthOffsetAction"));
    auto *antennaGainAction = settings->addAction(
        tr("Antenna gain..."), this, &MainWindow::openAntennaGain);
    antennaGainAction->setObjectName(QStringLiteral("antennaGainAction"));
    auto *windWeatherSettingsAction = settings->addAction(
        tr("Wind data source..."), this, &MainWindow::openWindWeatherSettings);
    windWeatherSettingsAction->setObjectName(QStringLiteral("windWeatherSettingsAction"));
    auto *shortestPathAction = settings->addAction(tr("Shortest rotation path"));
    shortestPathAction->setObjectName(QStringLiteral("shortestPathAction"));
    shortestPathAction->setCheckable(true);
    shortestPathAction->setChecked(preferences.shortestPathEnabled());
    connect(shortestPathAction, &QAction::toggled, this, [this](bool enabled) {
        m_control->setShortestPathEnabled(enabled);
        m_n1mm->setShortestPathEnabled(enabled);
        m_gridTracker->setShortestPathEnabled(enabled);
        m_wsjtX->setShortestPathEnabled(enabled);
        if (!AppSettings().setShortestPathEnabled(enabled))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
    });
    settings->addSeparator();
    m_speedMenu = settings->addMenu(tr("Azimuth speed"));
    m_speedMenu->setObjectName(QStringLiteral("azimuthSpeedMenu"));
    auto *speeds = new QActionGroup(m_speedMenu);
    speeds->setExclusive(true);
    for (int speed = 1; speed <= 4; ++speed) {
        const QString command = QStringLiteral("X%1").arg(speed);
        auto *action = m_speedMenu->addAction(command);
        action->setObjectName(QStringLiteral("speed%1").arg(speed));
        action->setCheckable(true);
        speeds->addAction(action);
        connect(action, &QAction::triggered, this, [this, command] {
            sendControlCommand(command);
        });
    }
    m_rotorModelAction = settings->addAction(
        tr("Rotor model..."), this, &MainWindow::openRotorModel);
    m_rotorModelAction->setObjectName(QStringLiteral("rotorModelAction"));
    m_calibrationAction = settings->addAction(
        tr("Calibration..."), this, &MainWindow::openCalibration);
    m_calibrationAction->setObjectName(QStringLiteral("calibrationAction"));
    auto *management = menuBar()->addMenu(tr("Management"));
    m_authenticateAction = management->addAction(
        tr("Authenticate"), this, &MainWindow::authenticateManagement);
    m_authenticateAction->setObjectName(QStringLiteral("authenticateAction"));
    m_changePasswordAction = management->addAction(
        tr("Change password"), this, &MainWindow::changeManagementPassword);
    m_changePasswordAction->setObjectName(QStringLiteral("changePasswordAction"));
    m_cancelManagementAction = management->addAction(
        tr("Cancel management"), this, &MainWindow::cancelManagement);
    m_cancelManagementAction->setObjectName(QStringLiteral("cancelManagementAction"));
    m_getIpAddressesAction = management->addAction(
        tr("Get IP"), this, &MainWindow::getIpAddresses);
    m_getIpAddressesAction->setObjectName(QStringLiteral("getIpAddressesAction"));
    auto *wifiMenu = management->addMenu(tr("Wi-Fi provisioning"));
    m_startApProvisioningAction = wifiMenu->addAction(
        tr("Start AP provisioning"), this, &MainWindow::startApProvisioning);
    m_startApProvisioningAction->setObjectName(QStringLiteral("startApProvisioningAction"));
    m_manualWifiAction = wifiMenu->addAction(
        tr("Manual provisioning"), this, &MainWindow::openWifiProvisioning);
    m_manualWifiAction->setObjectName(QStringLiteral("wifiAction"));
    m_networkPortAction = management->addAction(
        tr("Network port configuration"), this, &MainWindow::openNetworkPortConfig);
    m_networkPortAction->setObjectName(QStringLiteral("networkPortAction"));
    m_resetNetworkAction = management->addAction(
        tr("Reset network configuration"), this, &MainWindow::resetNetworkConfiguration);
    m_resetNetworkAction->setObjectName(QStringLiteral("resetNetworkAction"));
    auto *bridge = menuBar()->addMenu(tr("Bridge"));
    createN1mmMenu(bridge);
    createGridTrackerMenu(bridge);
    createWsjtXMenu(bridge);
    auto *upgrade = menuBar()->addMenu(tr("Upgrade"));
    m_upgradeAction = upgrade->addAction(tr("Flash firmware..."),
                                        this, &MainWindow::openFirmwareUpgrade);
    m_upgradeAction->setObjectName(QStringLiteral("upgradeAction"));
    auto *about = menuBar()->addMenu(tr("About"));
    auto *aboutAction = about->addAction(tr("GS-232B Open Control"),
                                        this, &MainWindow::openAbout);
    aboutAction->setObjectName(QStringLiteral("aboutAction"));
}

void MainWindow::setDevicePosition(int azimuth, int elevation)
{
    // NOTE: 校准保留设备原值，避免越过满量程的反馈被主控件钳位为 450° 或 180°。
    m_controllerAzimuth = azimuth;
    m_controllerElevation = elevation;
    m_wsjtX->setPosition(azimuth);
    m_control->setPosition(azimuth, elevation);
    m_windProtection->setCurrentAzimuth(azimuth);
    updateCalibrationPosition();
}

void MainWindow::openWindWeatherSettings()
{
    auto *dialog = new WindWeatherSettingsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this] {
        const AppSettings settings;
        const WindWeather::Provider provider = settings.windWeatherProvider();
        m_control->setWindWeatherProvider(provider);
        m_windProtection->setWeatherConfiguration(provider,
                                                  settings.qWeatherApiHost(),
                                                  settings.qWeatherApiKey(),
                                                  settings.windWeatherTermsAccepted());
        updateBridgeState();
    });
    dialog->open();
}

void MainWindow::authenticateManagement()
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;
    if (!m_connection->isNetworkMode()) {
        QMessageBox::information(this, tr("Management authentication"),
                                 tr("Serial connections do not require management authentication."));
        return;
    }
    openManagementKey([this] {
        m_console->appendNotice(tr("Management authentication succeeded."));
    });
}

void MainWindow::changeManagementPassword()
{
    runManagementAction([this] {
        auto *dialog = new PasswordChangeDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &QDialog::accepted, this, [this, dialog] {
            QJsonObject request;
            request.insert(QStringLiteral("password"), dialog->password());
            sendManagementCommand(
                QStringLiteral("!ADMIN SET %1")
                    .arg(QString::fromUtf8(
                        QJsonDocument(request).toJson(QJsonDocument::Compact))),
                QStringLiteral("ADMIN SET"), [this](const QJsonObject &) {
                    m_managementAuthenticated = false;
                    m_wifiTrialPollTimer->stop();
                    m_networkConfigurationStage = NetworkConfigurationStage::Idle;
                    m_pendingNetworkPatch = {};
                    m_pendingWifiSsid.clear();
                    m_console->appendNotice(tr("Management password changed."));
                });
        });
        dialog->open();
    });
}

void MainWindow::cancelManagement()
{
    runManagementAction([this] {
        sendManagementCommand(QStringLiteral("!ADMIN DISABLE"), QStringLiteral("ADMIN DISABLE"),
                              [this](const QJsonObject &) {
                                  m_managementAuthenticated = false;
                                  m_wifiTrialPollTimer->stop();
                                  m_networkConfigurationStage = NetworkConfigurationStage::Idle;
                                  m_pendingNetworkPatch = {};
                                  m_pendingWifiSsid.clear();
                                  m_console->appendNotice(tr("Management authentication disabled."));
                              });
    });
}

void MainWindow::startApProvisioning()
{
    runManagementAction([this] {
        sendManagementCommand(
            QStringLiteral("!PROV START"), QStringLiteral("PROV START"),
            [this](const QJsonObject &data) {
                const QJsonValue remaining = data.value(QStringLiteral("remaining_s"));
                const int seconds = remaining.toInt(-1);
                if (remaining.isDouble() && seconds >= 0 && remaining.toDouble() == seconds) {
                    m_console->appendNotice(
                        tr("AP provisioning started. Window remaining: %1 s.").arg(seconds));
                } else {
                    m_console->appendNotice(tr("AP provisioning started."));
                }
            });
    });
}

void MainWindow::openStationGrid()
{
    auto *dialog = new StationGridDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this] {
        const QString grid = AppSettings().stationGrid();
        m_control->setStationGrid(grid);
        m_wsjtX->setStationGrid(grid);
        m_windProtection->setStationGrid(grid);
        updateBridgeState();
    });
    dialog->open();
}

void MainWindow::openAzimuthOffset()
{
    auto *dialog = new AzimuthOffsetDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this] {
        m_control->setAzimuthOffset(AppSettings().azimuthOffset());
        m_n1mm->setAzimuthOffset(AppSettings().azimuthOffset());
        m_gridTracker->setAzimuthOffset(AppSettings().azimuthOffset());
        m_wsjtX->setAzimuthOffset(AppSettings().azimuthOffset());
        m_windProtection->setAzimuthOffset(AppSettings().azimuthOffset());
        updateCalibrationPosition();
    });
    dialog->open();
}

void MainWindow::openAntennaGain()
{
    auto *dialog = new AntennaGainDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this] {
        m_control->setAntennaGain(AppSettings().antennaGain());
    });
    dialog->open();
}

void MainWindow::openWifiProvisioning()
{
    runManagementAction([this] {
        auto *dialog = new WifiProvisioningDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &WifiProvisioningDialog::configurationRequested,
                this, &MainWindow::configureWifi);
        dialog->setConnectionState(m_connection->isConnected() && !m_connection->isVirtualComEnabled());
        dialog->open();
    });
}

void MainWindow::openNetworkPortConfig()
{
    runManagementAction([this] {
        auto *dialog = new NetworkPortConfigDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &NetworkPortConfigDialog::configurationRequested,
                this, &MainWindow::configureEthernet);
        dialog->setConnectionState(m_connection->isConnected() && !m_connection->isVirtualComEnabled());
        dialog->open();
    });
}

void MainWindow::resetNetworkConfiguration()
{
    runManagementAction([this] {
        if (QMessageBox::question(
                this, tr("Reset network configuration"),
                tr("Reset Ethernet and Wi-Fi settings and reopen AP provisioning?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        QJsonObject request;
        request.insert(QStringLiteral("confirm"), QStringLiteral("NETWORK_ONLY"));
        sendManagementCommand(
            QStringLiteral("!NET RESET %1")
                .arg(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact))),
            QStringLiteral("NET RESET"), [this](const QJsonObject &) {
                m_managementAuthenticated = false;
                m_wifiTrialPollTimer->stop();
                m_networkConfigurationStage = NetworkConfigurationStage::Idle;
                m_pendingNetworkPatch = {};
                m_pendingWifiSsid.clear();
                m_console->appendNotice(tr("Network configuration reset."));
            });
    });
}

void MainWindow::openFirmwareUpgrade()
{
    auto *dialog = new FirmwareUpgradeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &FirmwareUpgradeDialog::flashRequested,
            this, &MainWindow::flashFirmware);
    dialog->open();
}

void MainWindow::openAbout()
{
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("aboutDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("About"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->resize(360, 150);
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(new QLabel(tr("GS-232B Open Control"), dialog));
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(dialog);
    buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog->open();
}

void MainWindow::flashFirmware(const QString &port, const QString &file)
{
    if (port.isEmpty() || file.isEmpty())
        return;

    if (!m_connection->isNetworkMode() &&
        m_connection->connectionOptions().serialPort == port && m_transport->isOpen())
        closeTransport();

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        const QByteArray output = process->readAllStandardOutput();
        if (!output.isEmpty())
            m_console->appendProcessOutput(output);
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
                m_console->appendError(tr("Firmware flashing process failed to start."));
                if (error == QProcess::FailedToStart)
                    process->deleteLater();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus status) {
                if (status == QProcess::NormalExit && exitCode == 0) {
                    m_console->appendNotice(tr("Firmware flashing completed."));
                } else {
                    m_console->appendError(
                        tr("Firmware flashing failed with exit code %1.").arg(exitCode));
                }
                process->deleteLater();
            });

    const QStringList arguments = {
        QStringLiteral("--chip"), QStringLiteral("esp32c3"),
        QStringLiteral("--port"), port,
        QStringLiteral("--baud"), QStringLiteral("460800"),
        QStringLiteral("write-flash"), QStringLiteral("0x10000"), file};
    process->start(QStringLiteral("esptool"), arguments);
}

void MainWindow::sendControlCommand(const QString &command)
{
    if (command.trimmed().isEmpty() || !m_transport->isOpen())
        return;
    if (m_connection->isVirtualComEnabled()) {
        m_console->appendError(tr("Direct device commands are disabled in Virtual COM mode."));
        return;
    }
    const QByteArray bytes = Communication::encodeCommand(command);
    const QByteArray logBytes = Communication::commandForLog(bytes);
    const bool reportAsSent = logBytes == bytes;
    // NOTE: 普通命令沿用真实发送信号；敏感命令仅在发送成功后显示脱敏副本。
    if (m_transport->sendCommand(command, reportAsSent) && !reportAsSent)
        m_console->appendSent(logBytes);
}

void MainWindow::runManagementAction(const std::function<void()> &action,
                                     const std::function<void()> &cancelled)
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;
    if (m_connection->isNetworkMode())
        openManagementKey(action, cancelled);
    else
        action();
}

ManagementKeyDialog *MainWindow::openManagementKey(const std::function<void()> &continuation,
                                                  const std::function<void()> &cancelled,
                                                  QWidget *promptParent)
{
    if (m_managementAuthenticated) {
        continuation();
        return nullptr;
    }

    auto *dialog = new ManagementKeyDialog(promptParent ? promptParent : this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog, continuation] {
        const QString password = dialog->password();
        if (password.isEmpty())
            return;
        m_pendingManagementPassword = password;
        m_pendingManagementResponseCommand.clear();
        m_pendingManagementResponseContinuation = {};
        m_pendingManagementContinuation = continuation;
        sendControlCommand(managementAuthCommand(password));
    });
    connect(dialog, &QDialog::rejected, this, [cancelled] {
        if (cancelled)
            cancelled();
    });
    dialog->open();
    return dialog;
}

void MainWindow::sendManagementCommand(
    const QString &command, const QString &responseCommand,
    const std::function<void(const QJsonObject &)> &onSuccess)
{
    if (command.trimmed().isEmpty() || responseCommand.trimmed().isEmpty())
        return;
    m_pendingManagementResponseCommand = responseCommand.toUpper();
    m_pendingManagementResponseContinuation = onSuccess;
    sendControlCommand(command);
}

void MainWindow::configureWifi(const QString &ssid, const QString &password)
{
    if (ssid.isEmpty() || !m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    QJsonObject wifi;
    wifi.insert(QStringLiteral("enabled"), true);
    wifi.insert(QStringLiteral("ssid"), ssid);
    wifi.insert(QStringLiteral("password"), password);
    wifi.insert(QStringLiteral("security"), password.isEmpty()
                                             ? QStringLiteral("open")
                                             : QStringLiteral("auto"));
    QJsonObject patch;
    patch.insert(QStringLiteral("wifi"), wifi);
    beginNetworkConfiguration(patch, true, ssid);
}

void MainWindow::configureEthernet(const EthernetConfiguration &configuration)
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    QJsonObject ethernet;
    ethernet.insert(QStringLiteral("mode"), configuration.mode);
    if (configuration.mode == QStringLiteral("static")) {
        ethernet.insert(QStringLiteral("address"), configuration.address);
        ethernet.insert(QStringLiteral("netmask"), configuration.netmask);
        const auto insertIfSet = [&ethernet](const QString &name, const QString &value) {
            if (!value.isEmpty())
                ethernet.insert(name, value);
        };
        insertIfSet(QStringLiteral("gateway"), configuration.gateway);
        insertIfSet(QStringLiteral("dns1"), configuration.dns1);
        insertIfSet(QStringLiteral("dns2"), configuration.dns2);
    }
    QJsonObject patch;
    patch.insert(QStringLiteral("eth"), ethernet);
    beginNetworkConfiguration(patch, false);
}

void MainWindow::beginNetworkConfiguration(const QJsonObject &patch, bool wifiTarget,
                                           const QString &wifiSsid)
{
    if (patch.isEmpty() || !m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    m_pendingNetworkPatch = patch;
    m_pendingNetworkWifiTarget = wifiTarget;
    m_pendingWifiSsid = wifiSsid;
    m_pendingNetworkCandidate = -1;
    m_pendingNetworkRevision = -1;
    m_pendingNetworkTrial = -1;

    const auto begin = [this] {
        m_networkConfigurationStage = NetworkConfigurationStage::AwaitBegin;
        sendControlCommand(QStringLiteral("!CFG BEGIN"));
    };
    const auto cancel = [this] {
        m_pendingWifiSsid.clear();
        m_pendingNetworkPatch = {};
        m_pendingNetworkWifiTarget = true;
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
    };
    runManagementAction(begin, cancel);
}

void MainWindow::handleReceivedBytes(const QByteArray &bytes)
{
    if (m_connection->isVirtualComEnabled()) {
        if (!m_virtualSerial->feedWebSocketBytes(bytes))
            closeTransport();
        m_console->appendReceived(bytes);
        return;
    }
    m_receiveBuffer.append(bytes);
    constexpr qsizetype maximumBuffer = 64 * 1024;
    if (m_receiveBuffer.size() > maximumBuffer) {
        m_console->appendReceived(m_receiveBuffer.mid(m_consoleForwardedBytes));
        m_receiveBuffer.clear();
        m_consoleForwardedBytes = 0;
        return;
    }

    qsizetype newline = -1;
    while ((newline = m_receiveBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_receiveBuffer.left(newline);
        m_receiveBuffer.remove(0, newline + 1);
        const auto response = QJsonDocument::fromJson(line.trimmed().mid(1));
        const bool faultResponse = line.trimmed().startsWith('!') && response.isObject() &&
                                   m_faultStatus->handleResponse(response.object());
        // NOTE: 自动故障查询只更新状态面板，避免原始诊断字段淹没控制台。
        if (!faultResponse)
            m_console->appendReceived((line + '\n').mid(m_consoleForwardedBytes));
        m_consoleForwardedBytes = 0;
        handleManagementResponse(line);

        const QString text = QString::fromUtf8(line).trimmed();
        if (!m_pendingCalibrationCommand.isEmpty() &&
            text.contains(QStringLiteral("are you sure?"), Qt::CaseInsensitive)) {
            QWidget *promptParent = m_calibrationDialog ? static_cast<QWidget *>(m_calibrationDialog.data()) : this;
            const auto answer = QMessageBox::question(
                promptParent, tr("Confirm calibration"),
                tr("The device is ready to store this calibration point. Continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer == QMessageBox::Yes) {
                sendControlCommand(QStringLiteral("YES"));
                m_calibrationTimeoutTimer->start();
            } else {
                sendControlCommand(QStringLiteral("NO"));
                m_pendingCalibrationCommand.clear();
                updateBridgeState();
                updateCalibrationState();
                m_faultStatus->setPaused(false);
                m_calibrationTimeoutTimer->stop();
                if (m_transport->isOpen())
                    m_positionPollTimer->start();
            }
        } else if (!m_pendingCalibrationCommand.isEmpty() &&
                   (text.contains(QStringLiteral("Completed"), Qt::CaseInsensitive) ||
                    text.startsWith(QStringLiteral("?>")))) {
            m_pendingCalibrationCommand.clear();
            updateBridgeState();
            updateCalibrationState();
            m_faultStatus->setPaused(false);
            m_calibrationTimeoutTimer->stop();
            if (m_transport->isOpen())
                m_positionPollTimer->start();
        }
        static const QRegularExpression azimuthElevation(
            QStringLiteral("^AZ\\s*=\\s*(\\d{1,3})\\s+EL\\s*=\\s*(\\d{1,3})$"));
        static const QRegularExpression azimuthOnly(
            QStringLiteral("^AZ\\s*=\\s*(\\d{1,3})$"));
        static const QRegularExpression elevationOnly(
            QStringLiteral("^EL\\s*=\\s*(\\d{1,3})$"));
        const auto azElMatch = azimuthElevation.match(text);
        if (azElMatch.hasMatch()) {
            setDevicePosition(azElMatch.captured(1).toInt(),
                              azElMatch.captured(2).toInt());
            m_n1mm->setPosition(azElMatch.captured(1).toInt());
            m_gridTracker->setAzimuthPosition(azElMatch.captured(1).toInt());
            m_gridTracker->setElevationPosition(azElMatch.captured(2).toInt());
        } else if (const auto azMatch = azimuthOnly.match(text); azMatch.hasMatch()) {
            setDevicePosition(azMatch.captured(1).toInt(), m_controllerElevation);
            m_n1mm->setPosition(azMatch.captured(1).toInt());
            m_gridTracker->setAzimuthPosition(azMatch.captured(1).toInt());
        } else if (const auto elMatch = elevationOnly.match(text); elMatch.hasMatch()) {
            setDevicePosition(m_controllerAzimuth, elMatch.captured(1).toInt());
            m_gridTracker->setElevationPosition(elMatch.captured(1).toInt());
        }
    }
    // NOTE: 仅缓冲扩展响应以便按命令过滤；普通终端输出仍随字节到达立即显示。
    if (!m_receiveBuffer.trimmed().startsWith('!')) {
        m_console->appendReceived(m_receiveBuffer.mid(m_consoleForwardedBytes));
        m_consoleForwardedBytes = m_receiveBuffer.size();
    }
}

void MainWindow::handleManagementResponse(const QByteArray &line)
{
    const QByteArray trimmed = line.trimmed();
    if (!trimmed.startsWith('!'))
        return;

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(trimmed.mid(1), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

    const QJsonObject response = document.object();
    const QString command = response.value(QStringLiteral("cmd")).toString().toUpper();
    const bool ok = response.value(QStringLiteral("ok")).toBool();
    handleRotorModelResponse(response);
    if (!ok && response.value(QStringLiteral("error")).toString() ==
                  QStringLiteral("NOT_AUTHORIZED")) {
        m_managementAuthenticated = false;
        m_pendingManagementPassword.clear();
        m_pendingManagementContinuation = {};
    }
    if (command == QStringLiteral("SESSION AUTH")) {
        m_managementAuthenticated = ok;
        m_pendingManagementPassword.clear();
        if (m_managementAuthenticated) {
            const auto continuation = m_pendingManagementContinuation;
            m_pendingManagementContinuation = {};
            if (continuation)
                continuation();
        } else {
            m_pendingManagementContinuation = {};
            m_pendingManagementResponseCommand.clear();
            m_pendingManagementResponseContinuation = {};
            m_console->appendError(tr("Management authentication failed."));
            m_networkConfigurationStage = NetworkConfigurationStage::Idle;
            m_pendingNetworkPatch = {};
            m_pendingWifiSsid.clear();
            return;
        }
    }

    if (!m_pendingManagementResponseCommand.isEmpty() &&
        command == m_pendingManagementResponseCommand) {
        const auto continuation = m_pendingManagementResponseContinuation;
        m_pendingManagementResponseCommand.clear();
        m_pendingManagementResponseContinuation = {};
        if (!ok) {
            m_console->appendError(
                tr("Management command failed: %1")
                    .arg(response.value(QStringLiteral("error")).toString()));
            return;
        }
        if (continuation)
            continuation(response.value(QStringLiteral("data")).toObject());
        // NOTE: 获取 IP 与配网试运行共用 NET GET；查询回调不能吞掉试运行就绪状态。
        if (command != QStringLiteral("NET GET") ||
            m_networkConfigurationStage != NetworkConfigurationStage::AwaitRuntime)
            return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::Idle)
        return;

    QString expectedCommand;
    switch (m_networkConfigurationStage) {
    case NetworkConfigurationStage::AwaitBegin: expectedCommand = QStringLiteral("CFG BEGIN"); break;
    case NetworkConfigurationStage::AwaitSet: expectedCommand = QStringLiteral("NET SET"); break;
    case NetworkConfigurationStage::AwaitCheck: expectedCommand = QStringLiteral("CFG CHECK"); break;
    case NetworkConfigurationStage::AwaitApply: expectedCommand = QStringLiteral("CFG APPLY"); break;
    case NetworkConfigurationStage::AwaitRuntime: expectedCommand = QStringLiteral("NET GET"); break;
    case NetworkConfigurationStage::AwaitConfirm: expectedCommand = QStringLiteral("CFG CONFIRM"); break;
    case NetworkConfigurationStage::AwaitAbort: expectedCommand = QStringLiteral("CFG ABORT"); break;
    case NetworkConfigurationStage::Idle: return;
    }
    if (command != expectedCommand)
        return;

    if (!ok) {
        m_console->appendError(
            tr("Network configuration failed: %1")
                .arg(response.value(QStringLiteral("error")).toString()));
        m_wifiTrialPollTimer->stop();
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
        m_pendingNetworkPatch = {};
        m_pendingWifiSsid.clear();
        return;
    }

    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitBegin &&
        command == QStringLiteral("CFG BEGIN")) {
        m_pendingNetworkCandidate = configurationId(data.value(QStringLiteral("candidate_id")));
        m_pendingNetworkRevision = configurationId(data.value(QStringLiteral("base_revision")));
        if (m_pendingNetworkCandidate < 0) {
            m_console->appendError(tr("Device did not return a network candidate ID."));
            m_networkConfigurationStage = NetworkConfigurationStage::Idle;
            m_pendingNetworkPatch = {};
            m_pendingWifiSsid.clear();
            return;
        }

        m_networkConfigurationStage = NetworkConfigurationStage::AwaitSet;
        sendControlCommand(QStringLiteral("!NET SET %1")
                               .arg(QString::fromUtf8(
                                   QJsonDocument(m_pendingNetworkPatch)
                                       .toJson(QJsonDocument::Compact))));
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitSet &&
        command == QStringLiteral("NET SET")) {
        m_pendingNetworkPatch = {};
        m_networkConfigurationStage = NetworkConfigurationStage::AwaitCheck;
        sendControlCommand(QStringLiteral("!CFG CHECK"));
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitCheck &&
        command == QStringLiteral("CFG CHECK")) {
        QJsonObject request;
        request.insert(QStringLiteral("candidate_id"), m_pendingNetworkCandidate);
        if (m_pendingNetworkRevision >= 0)
            request.insert(QStringLiteral("revision"), m_pendingNetworkRevision);
        request.insert(QStringLiteral("timeout_s"), 300);
        m_networkConfigurationStage = NetworkConfigurationStage::AwaitApply;
        sendControlCommand(QStringLiteral("!CFG APPLY %1")
                               .arg(QString::fromUtf8(
                                   QJsonDocument(request).toJson(QJsonDocument::Compact))));
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitApply &&
        command == QStringLiteral("CFG APPLY")) {
        m_pendingNetworkTrial = configurationId(data.value(QStringLiteral("trial_id")));
        if (m_pendingNetworkTrial < 0) {
            m_console->appendError(tr("Device did not return a network trial ID."));
            m_networkConfigurationStage = NetworkConfigurationStage::Idle;
            m_pendingNetworkPatch = {};
            m_pendingWifiSsid.clear();
            return;
        }

        m_networkConfigurationStage = NetworkConfigurationStage::AwaitRuntime;
        m_wifiTrialPollCount = 0;
        m_wifiTrialPollTimer->start();
        sendControlCommand(QStringLiteral("!NET GET"));
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitRuntime &&
        command == QStringLiteral("NET GET")) {
        const QJsonObject runtime = data.value(QStringLiteral("runtime")).toObject();
        bool ready = false;
        if (m_pendingNetworkWifiTarget) {
            const QJsonObject wifi = runtime.value(QStringLiteral("wifi")).toObject();
            ready = wifi.value(QStringLiteral("connected")).toBool() &&
                    wifi.value(QStringLiteral("ipv4_valid")).toBool() &&
                    wifi.value(QStringLiteral("ssid")).toString() == m_pendingWifiSsid;
        } else {
            const QJsonObject ethernet = runtime.value(QStringLiteral("eth")).toObject();
            ready = ethernet.value(QStringLiteral("ipv4_valid")).toBool();
        }
        if (!ready)
            return;

        m_wifiTrialPollTimer->stop();
        m_networkConfigurationStage = NetworkConfigurationStage::AwaitConfirm;
        const QString title = m_pendingNetworkWifiTarget
            ? tr("Confirm Wi-Fi configuration")
            : tr("Confirm network port configuration");
        const QString question = m_pendingNetworkWifiTarget
            ? tr("The new Wi-Fi network is connected and has an IP address. Confirm and save it?")
            : tr("The network port has a valid IPv4 address. Confirm and save it?");
        const auto answer = QMessageBox::question(
            this, title, question,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            QJsonObject request;
            request.insert(QStringLiteral("trial_id"), m_pendingNetworkTrial);
            sendControlCommand(QStringLiteral("!CFG CONFIRM %1")
                                   .arg(QString::fromUtf8(
                                       QJsonDocument(request).toJson(QJsonDocument::Compact))));
        } else {
            QJsonObject request;
            request.insert(QStringLiteral("trial_id"), m_pendingNetworkTrial);
            m_networkConfigurationStage = NetworkConfigurationStage::AwaitAbort;
            sendControlCommand(QStringLiteral("!CFG ABORT %1")
                                   .arg(QString::fromUtf8(
                                       QJsonDocument(request).toJson(QJsonDocument::Compact))));
        }
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitAbort &&
        command == QStringLiteral("CFG ABORT")) {
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
        m_pendingNetworkPatch = {};
        m_pendingWifiSsid.clear();
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitConfirm &&
        command == QStringLiteral("CFG CONFIRM")) {
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
        m_pendingNetworkPatch = {};
        m_pendingWifiSsid.clear();
        m_console->appendNotice(m_pendingNetworkWifiTarget
            ? tr("Wi-Fi configuration saved.")
            : tr("Network port configuration saved."));
    }
}
