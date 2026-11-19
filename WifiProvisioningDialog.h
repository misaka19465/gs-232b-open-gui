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

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;

/** @brief Wi-Fi 配网的独立 Widgets 对话框原型。 */
class WifiProvisioningDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建 SSID/口令输入视图，不建立网络连接。 */
    explicit WifiProvisioningDialog(QWidget *parent = nullptr);
    /** @brief 按连接和透传状态更新配置按钮。 */
    void setConnectionState(bool commandEnabled);

signals:
    /** @brief 用户提交 Wi-Fi 配置，具体管理命令由主窗口按响应顺序发送。 */
    void configurationRequested(const QString &ssid, const QString &password);

private:
    void updateControls();

    bool m_commandEnabled = false;
    QLineEdit *m_ssid;
    QLineEdit *m_password;
    QPushButton *m_configure;
};
