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
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "ConsolePanel.h"
#include "ControlPanel.h"
#include "FaultStatusMonitor.h"
#include "virtual_serial/VirtualSerialBridge.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QShowEvent>
#include <QTimer>

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (!m_initialConnectionPrompt)
        return;
    m_initialConnectionPrompt = false;
    // NOTE: 等主窗口显示后再打开子对话框，确保模态关系和居中位置正确。
    QTimer::singleShot(0, this, &MainWindow::openConnectionDialog);
}

void MainWindow::openConnectionDialog()
{
    if (!isVisible() || m_connection->isConnected())
        return;
    if (!m_connection->isVisible())
        m_connection->open();
    m_connection->raise();
    m_connection->activateWindow();
}

void MainWindow::updateConnectionState()
{
    const bool connected = m_connection->isConnected();
    const bool connecting = m_connection->isConnecting();
    m_connectionAction->setText(connected ? tr("Disconnect") : tr("Connect"));
    m_connectionAction->setEnabled(!connecting);
    updateBridgeState();
    const bool virtualCom = m_connection->isVirtualComEnabled();
    const bool commandEnabled = m_connection->isConnected() && !virtualCom;
    m_console->setCommandEnabled(commandEnabled);
    m_speedMenu->setEnabled(commandEnabled);
    updateRotorModelState();
    updateCalibrationState();
    m_faultStatus->setEnabled(commandEnabled);
    const bool managementEnabled = m_connection->isConnected() && !virtualCom;
    m_authenticateAction->setEnabled(managementEnabled);
    m_changePasswordAction->setEnabled(managementEnabled);
    m_cancelManagementAction->setEnabled(managementEnabled);
    m_getIpAddressesAction->setEnabled(managementEnabled);
    m_startApProvisioningAction->setEnabled(managementEnabled);
    m_manualWifiAction->setEnabled(managementEnabled);
    m_networkPortAction->setEnabled(managementEnabled);
    m_resetNetworkAction->setEnabled(managementEnabled);
    m_upgradeAction->setEnabled(!virtualCom);
    if (commandEnabled)
        m_positionPollTimer->start();
    else
        m_positionPollTimer->stop();
    m_connectionStatus->setText(connecting ? tr("Connecting...")
                                         : connected ? tr("Connected") : tr("Disconnected"));
    m_transportStatus->setText(virtualCom && m_virtualSerial->isRunning()
                                  ? tr("WebSocket (%1)").arg(m_virtualSerial->clientPortName())
                                  : m_connection->isNetworkMode() ? tr("WebSocket") : tr("Serial"));
    if (!m_connection->isConnected())
        setDevicePosition(0, 0);
}

void MainWindow::openTransport()
{
    if (m_transport->isOpen() || m_connection->isConnecting())
        return;

    m_connection->setConnecting(true);
    // NOTE: 主页面终端固定使用控制端点；管理端点由协议模块保留给后续独立调用方。
    const auto options = m_connection->connectionOptions(Communication::WebSocketEndpoint::Gs232b);
    if (!m_transport->open(options))
        m_connection->setConnecting(false);
}

void MainWindow::closeTransport()
{
    m_control->cancelPendingMotion();
    m_transport->close();
    m_virtualSerial->stop();
    m_connection->setConnecting(false);
    m_connection->setConnected(false);
}

