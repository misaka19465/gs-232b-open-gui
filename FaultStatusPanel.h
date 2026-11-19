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

#pragma once

#include "FaultStatusMonitor.h"

#include <QToolButton>

class QDialog;
class QLabel;
class QPushButton;

/** @brief 在状态栏显示故障摘要，点击后打开包含故障详情及清除操作的非模态窗口。 */
class FaultStatusPanel final : public QToolButton
{
    Q_OBJECT

public:
    /** @brief 绑定宿主管理的故障监视器；监视器须覆盖控件及详情窗口的生命周期。 */
    explicit FaultStatusPanel(FaultStatusMonitor *monitor, QWidget *parent = nullptr);

private:
    void updateStatus();
    void updateIndicator();
    void showClearResult(FaultStatusMonitor::ClearResult result);
    QString faultDescription() const;

    FaultStatusMonitor *m_monitor;
    QDialog *m_details;
    QLabel *m_status;
    QLabel *m_result;
    QPushButton *m_clear;
};
