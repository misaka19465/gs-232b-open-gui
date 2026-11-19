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
#include "ConsolePanel.h"
#include "ControlPanel.h"

#include <QAction>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>

void MainWindow::createViewMenu()
{
    const AppSettings preferences;
    auto *view = menuBar()->addMenu(tr("View"));
    auto *consoleAction = view->addAction(tr("Console"));
    consoleAction->setObjectName(QStringLiteral("consoleAction"));
    consoleAction->setCheckable(true);
    consoleAction->setChecked(preferences.consoleVisible());
    m_console->setVisible(consoleAction->isChecked());
    // NOTE: 恢复状态后再连接保存，启动读取不覆盖缺失或无效的配置值。
    connect(consoleAction, &QAction::toggled, this, [this](bool visible) {
        const int consoleHeight = m_console->height() + centralWidget()->layout()->spacing();
        const int targetHeight = height() + (visible ? consoleHeight : -consoleHeight);
        m_console->setVisible(visible);
        // NOTE: 先更新布局的最小高度，再同步增减窗口高度，保留主控制区尺寸。
        centralWidget()->layout()->activate();
        layout()->activate();
        if (!isMaximized() && !isFullScreen())
            resize(width(), targetHeight);
        if (!AppSettings().setConsoleVisible(visible))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
    });
    auto *mapAction = view->addAction(tr("Great-circle map"));
    mapAction->setObjectName(QStringLiteral("greatCircleMapAction"));
    mapAction->setCheckable(true);
    m_control->setStationGrid(preferences.stationGrid());
    mapAction->setChecked(preferences.greatCircleMapEnabled());
    m_control->setGreatCircleMapEnabled(mapAction->isChecked());
    connect(mapAction, &QAction::toggled, this, [this](bool enabled) {
        m_control->setGreatCircleMapEnabled(enabled);
        if (!AppSettings().setGreatCircleMapEnabled(enabled))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
        if (enabled && AppSettings().stationGrid().isEmpty())
            openStationGrid();
    });
    view->addSeparator();
    auto *alwaysOnTopAction = view->addAction(tr("Always on top"));
    alwaysOnTopAction->setObjectName(QStringLiteral("alwaysOnTopAction"));
    alwaysOnTopAction->setCheckable(true);
    alwaysOnTopAction->setChecked(preferences.alwaysOnTop());
    setAlwaysOnTop(alwaysOnTopAction->isChecked());
    connect(alwaysOnTopAction, &QAction::toggled, this, [this](bool enabled) {
        setAlwaysOnTop(enabled);
        if (!AppSettings().setAlwaysOnTop(enabled))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
    });
}

void MainWindow::setElevationSupported(bool supported)
{
    if (m_control->elevationSupported() == supported)
        return;

    const int controlWidth = m_control->width();
    m_control->setElevationSupported(supported);
    centralWidget()->layout()->activate();
    layout()->activate();
    if (!m_windowSizeRestored || isMaximized() || isFullScreen())
        return;

    const int widthAdjustment = supported ? controlWidth : -(controlWidth / 2);
    resize(width() + widthAdjustment, height());
}

void MainWindow::setAlwaysOnTop(bool enabled)
{
    if (windowFlags().testFlag(Qt::WindowStaysOnTopHint) == enabled)
        return;

    const bool wasVisible = isVisible();
    const Qt::WindowStates state = windowState();
    // NOTE: 修改窗口标志会隐藏原生窗口；恢复状态后重新显示，避免切换置顶时窗口消失。
    setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    setWindowState(state);
    if (wasVisible)
        show();
}
