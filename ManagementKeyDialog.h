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

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;

/** @brief 仅在网络管理操作需要认证时打开的独立密码窗口。 */
class ManagementKeyDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建密码输入视图；不在连接时自动调用。 */
    explicit ManagementKeyDialog(QWidget *parent = nullptr);
    /** @return 用户输入的当前会话管理密码。 */
    QString password() const;

private:
    void updateControls();

    QLineEdit *m_password;
    QPushButton *m_continue;
};
