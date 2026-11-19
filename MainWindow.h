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

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QPointer>
#include <QSize>
#include <QString>

#include <functional>

class QAction;
class CalibrationDialog;
class ConnectionDialog;
class ConsolePanel;
class ControlPanel;
class FaultStatusMonitor;
class GridTrackerController;
class QLabel;
class ManagementKeyDialog;
class QMenu;
class QCloseEvent;
class QResizeEvent;
class RotorModelDialog;
class QShowEvent;
class QTimer;
class VirtualSerialBridge;
class N1mmController;
class WsjtXController;
class WindProtectionController;
struct EthernetConfiguration;

namespace Communication {
class Transport;
}

/** @brief 使用系统样式的纯 Qt Widgets 单窗口上位机原型。 */
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    /** @brief 创建主页面和菜单。 */
    explicit MainWindow(QWidget *parent = nullptr);
    /** @brief 在界面销毁前关闭传输和虚拟串口。 */
    ~MainWindow() override;

private:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void saveWindowSize(bool warnOnFailure);
    void openConnectionDialog();
    void createMenus();
    void createViewMenu();
    /** @brief 应用窗口置顶标志，保留可见性和最大化等窗口状态。 */
    void setAlwaysOnTop(bool enabled);
    void createN1mmMenu(QMenu *bridge);
    void createGridTrackerMenu(QMenu *bridge);
    void createWsjtXMenu(QMenu *bridge);
    void updateBridgeState();
    void openN1mmName();
    void updateConnectionState();
    /** @brief 将设备原始角度同步到主控件和校准窗口，保留超出主控件显示范围的校准反馈。 */
    void setDevicePosition(int azimuth, int elevation);
    void openTransport();
    void closeTransport();
    void setupVirtualSerial();
    void authenticateManagement();
    void changeManagementPassword();
    void cancelManagement();
    void getIpAddresses();
    void startApProvisioning();
    void openStationGrid();
    void openWindWeatherSettings();
    void openAzimuthOffset();
    void openAntennaGain();
    void openCalibration();
    void updateCalibrationState();
    void updateCalibrationPosition();
    void openWifiProvisioning();
    void openNetworkPortConfig();
    void resetNetworkConfiguration();
    void openFirmwareUpgrade();
    void openAbout();
    void setupRotorModel();
    void setElevationSupported(bool supported);
    void openRotorModel();
    void updateRotorModelState();
    void applyRotorModel(int modelId);
    void finishRotorModelRequest(const QString &error = {});
    void handleRotorModelResponse(const QJsonObject &response);
    void flashFirmware(const QString &port, const QString &file);
    void sendControlCommand(const QString &command);
    void requestCalibration(const QString &command, const QString &question);
    void runManagementAction(const std::function<void()> &action,
                             const std::function<void()> &cancelled = {});
    /** @brief 按需认证；返回新建的密码窗口，已认证时立即继续并返回 nullptr。 */
    ManagementKeyDialog *openManagementKey(const std::function<void()> &continuation,
                                           const std::function<void()> &cancelled = {},
                                           QWidget *promptParent = nullptr);
    void configureWifi(const QString &ssid, const QString &password);
    void configureEthernet(const EthernetConfiguration &configuration);
    void beginNetworkConfiguration(const QJsonObject &patch, bool wifiTarget,
                                   const QString &wifiSsid = {});
    /** @brief 发送管理命令，仅将匹配的成功响应中的 data 交给回调。 */
    void sendManagementCommand(
        const QString &command, const QString &responseCommand,
        const std::function<void(const QJsonObject &)> &onSuccess = {});
    void handleReceivedBytes(const QByteArray &bytes);
    void handleManagementResponse(const QByteArray &line);

    ConnectionDialog *m_connection;
    ControlPanel *m_control;
    ConsolePanel *m_console;
    QMenu *m_speedMenu;
    RotorModelDialog *m_rotorModelDialog;
    QPointer<ManagementKeyDialog> m_rotorModelAuthDialog;
    QPointer<CalibrationDialog> m_calibrationDialog;
    FaultStatusMonitor *m_faultStatus;
    QLabel *m_connectionStatus;
    QLabel *m_transportStatus;
    Communication::Transport *m_transport;
    VirtualSerialBridge *m_virtualSerial;
    N1mmController *m_n1mm;
    GridTrackerController *m_gridTracker;
    WsjtXController *m_wsjtX;
    WindProtectionController *m_windProtection;
    QAction *m_n1mmEnableAction;
    QAction *m_connectionAction;
    QAction *m_authenticateAction;
    QAction *m_changePasswordAction;
    QAction *m_cancelManagementAction;
    QAction *m_getIpAddressesAction;
    QAction *m_startApProvisioningAction;
    QAction *m_manualWifiAction;
    QAction *m_networkPortAction;
    QAction *m_resetNetworkAction;
    QAction *m_upgradeAction;
    QAction *m_calibrationAction;
    QAction *m_rotorModelAction;
    QByteArray m_receiveBuffer;
    qsizetype m_consoleForwardedBytes = 0;
    int m_controllerAzimuth = 0;
    int m_controllerElevation = 0;
    QString m_pendingWifiSsid;
    QJsonObject m_pendingNetworkPatch;
    qint64 m_pendingNetworkCandidate = -1;
    qint64 m_pendingNetworkRevision = -1;
    qint64 m_pendingNetworkTrial = -1;
    QString m_pendingCalibrationCommand;
    QString m_pendingManagementPassword;
    QString m_pendingManagementResponseCommand;
    bool m_managementAuthenticated = false;
    bool m_initialConnectionPrompt = true;
    bool m_connectionPromptSuppressed = false;
    int m_pendingRotorModelId = 0;
    quint64 m_rotorModelRequest = 0;
    bool m_rotorModelWriteSent = false;
    std::function<void()> m_pendingManagementContinuation;
    std::function<void(const QJsonObject &)> m_pendingManagementResponseContinuation;
    QTimer *m_positionPollTimer;
    QTimer *m_wifiTrialPollTimer;
    QTimer *m_calibrationTimeoutTimer;
    QTimer *m_rotorModelTimeoutTimer;
    int m_wifiTrialPollCount = 0;
    QSize m_persistedWindowSize;
    bool m_windowSizeRestored = false;
    bool m_windowSizeSaved = false;

    enum class NetworkConfigurationStage {
        Idle,
        AwaitBegin,
        AwaitSet,
        AwaitCheck,
        AwaitApply,
        AwaitRuntime,
        AwaitConfirm,
        AwaitAbort,
    };
    NetworkConfigurationStage m_networkConfigurationStage = NetworkConfigurationStage::Idle;
    bool m_pendingNetworkWifiTarget = true;
};
