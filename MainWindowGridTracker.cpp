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

#include "MainWindow.h"
#include "AppSettings.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "ConsolePanel.h"
#include "ControlPanel.h"
#include "gridtracker/GridTrackerController.h"
#include "n1mm/N1mmController.h"
#include "wsjtx/WsjtXController.h"
#include "weather/WindProtectionController.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>

void MainWindow::createGridTrackerMenu(QMenu *bridge)
{
    m_gridTracker = new GridTrackerController(this);
    const AppSettings preferences;
    m_gridTracker->setAzimuthOffset(preferences.azimuthOffset());
    m_gridTracker->setShortestPathEnabled(preferences.shortestPathEnabled());
    m_gridTracker->setLongPathEnabled(m_control->longPathEnabled());
    connect(m_control, &ControlPanel::longPathEnabledChanged,
            m_gridTracker, &GridTrackerController::setLongPathEnabled);
    connect(m_gridTracker, &GridTrackerController::commandRequested, this, [this](const QString &command) {
        updateBridgeState();
        if (!m_transport->isOpen() || !m_connection->isConnected() ||
            m_connection->isVirtualComEnabled() || !m_pendingCalibrationCommand.isEmpty() ||
            !m_control->trackingEnabled())
            return;
        m_control->cancelPendingMotion();
        sendControlCommand(command);
    });
    connect(m_gridTracker, &GridTrackerController::errorOccurred, this, [this](const QString &message) {
        m_console->appendError(message);
        statusBar()->showMessage(message, 15000);
    });

    auto *menu = bridge->addMenu(tr("GridTracker..."));
    menu->setObjectName(QStringLiteral("gridTrackerMenu"));
    auto *enable = menu->addAction(tr("Enable"));
    enable->setObjectName(QStringLiteral("gridTrackerEnableAction"));
    enable->setCheckable(true);
    enable->setToolTip(tr("Receive CatRotator / PstRotator commands on 127.0.0.1:12000."));

    // NOTE: 启动恢复不回写偏好；端口冲突时保持界面关闭，主窗口显示后提示原因。
    if (preferences.gridTrackerEnabled()) {
        const bool enabled = m_gridTracker->setEnabled(true);
        enable->setChecked(enabled);
        if (!enabled) {
            const QString error = m_gridTracker->errorString();
            QTimer::singleShot(0, this, [this, error] {
                QMessageBox::warning(this, tr("GridTracker"), error);
            });
        }
    }
    connect(enable, &QAction::toggled, this, [this, enable](bool enabled) {
        if (!m_gridTracker->setEnabled(enabled)) {
            const QSignalBlocker blocker(enable);
            enable->setChecked(false);
            QMessageBox::warning(this, tr("GridTracker"), m_gridTracker->errorString());
            return;
        }
        if (!AppSettings().setGridTrackerEnabled(enabled))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
    });
}

void MainWindow::updateBridgeState()
{
    const bool available = m_transport->isOpen() && m_connection->isConnected() &&
                           !m_connection->isVirtualComEnabled() &&
                           m_pendingCalibrationCommand.isEmpty() &&
                           !(m_control->windProtectionEnabled() &&
                             m_windProtection->configurationReady());
    m_n1mm->setControlAvailable(available);
    m_gridTracker->setControlAvailable(available);
    m_wsjtX->setControlAvailable(available);
    m_n1mm->setTrackingEnabled(m_control->trackingEnabled());
    m_gridTracker->setTrackingEnabled(m_control->trackingEnabled());
    m_wsjtX->setTrackingEnabled(m_control->trackingEnabled());
}
