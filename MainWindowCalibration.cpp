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
#include "AppSettings.h"
#include "AzimuthScale.h"
#include "CalibrationDialog.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "ControlPanel.h"
#include "FaultStatusMonitor.h"
#include "n1mm/N1mmController.h"
#include "gridtracker/GridTrackerController.h"
#include "wsjtx/WsjtXController.h"
#include "weather/WindProtectionController.h"

#include <QAction>
#include <QMessageBox>
#include <QTimer>

void MainWindow::openCalibration()
{
    if (!m_transport->isOpen() || !m_connection->isConnected() ||
        m_connection->isVirtualComEnabled())
        return;
    if (!m_calibrationDialog) {
        m_calibrationDialog = new CalibrationDialog(this);
        m_calibrationDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_calibrationDialog, &CalibrationDialog::azimuthZeroCalibrationRequested,
                this, [this] {
                    requestCalibration(
                        QStringLiteral("O"),
                        tr("Move the azimuth to the counter-clockwise limit, then continue."));
                });
        connect(m_calibrationDialog, &CalibrationDialog::elevationZeroCalibrationRequested,
                this, [this] {
                    requestCalibration(
                        QStringLiteral("O2"),
                        tr("Move the elevation to 0 degrees, then continue."));
                });
    }
    updateCalibrationPosition();
    updateCalibrationState();
    m_calibrationDialog->show();
    m_calibrationDialog->raise();
    m_calibrationDialog->activateWindow();
}

void MainWindow::updateCalibrationState()
{
    const bool enabled = m_transport->isOpen() && m_connection->isConnected() &&
                         !m_connection->isVirtualComEnabled();
    const bool controlEnabled = enabled && m_pendingCalibrationCommand.isEmpty();
    m_control->setCommandEnabled(controlEnabled);
    m_windProtection->setConnected(controlEnabled);
    m_calibrationAction->setEnabled(enabled);
    if (m_calibrationDialog)
        m_calibrationDialog->setCommandEnabled(enabled && m_pendingCalibrationCommand.isEmpty());
}

void MainWindow::updateCalibrationPosition()
{
    if (m_calibrationDialog) {
        // NOTE: 校准沿用方位补偿但保留连续行程，不受最短路径显示开关影响。
        const AzimuthScale scale(AppSettings().azimuthOffset());
        m_calibrationDialog->setPosition(scale.corrected(m_controllerAzimuth), m_controllerElevation);
    }
}

void MainWindow::requestCalibration(const QString &command, const QString &question)
{
    if (!m_transport->isOpen() || !m_connection->isConnected() ||
        m_connection->isVirtualComEnabled() || !m_pendingCalibrationCommand.isEmpty())
        return;
    // NOTE: 确认弹窗也暂停外部定位，避免用户确认零点前被桥接或延迟滑条命令转离。
    m_n1mm->setControlAvailable(false);
    m_gridTracker->setControlAvailable(false);
    m_wsjtX->setControlAvailable(false);
    m_control->cancelPendingMotion();
    QWidget *promptParent = m_calibrationDialog ? static_cast<QWidget *>(m_calibrationDialog.data()) : this;
    const auto answer = QMessageBox::question(
        promptParent, tr("Calibration confirmation"), question,
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
    // NOTE: 确认期间可能断线或切换透传模式，发送前重新检查设备状态。
    if (answer == QMessageBox::Ok && m_transport->isOpen() &&
        m_connection->isConnected() && !m_connection->isVirtualComEnabled()) {
        m_pendingCalibrationCommand = command;
        updateBridgeState();
        updateCalibrationState();
        m_faultStatus->setPaused(true);
        m_positionPollTimer->stop();
        m_calibrationTimeoutTimer->start();
        sendControlCommand(command);
    } else {
        updateBridgeState();
    }
}
