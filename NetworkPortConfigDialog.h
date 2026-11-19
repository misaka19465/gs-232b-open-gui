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

class QComboBox;
class QLineEdit;
class QPushButton;

struct EthernetConfiguration
{
    QString mode;
    QString address;
    QString netmask;
    QString gateway;
    QString dns1;
    QString dns2;
};

/** @brief 编辑以太网 DHCP 或静态 IPv4 配置。 */
class NetworkPortConfigDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkPortConfigDialog(QWidget *parent = nullptr);
    void setConnectionState(bool commandEnabled);

signals:
    /** @brief 用户提交以太网配置，事务命令由主窗口发送。 */
    void configurationRequested(const EthernetConfiguration &configuration);

private:
    void updateControls();

    bool m_commandEnabled = false;
    QComboBox *m_mode;
    QLineEdit *m_address;
    QLineEdit *m_netmask;
    QLineEdit *m_gateway;
    QLineEdit *m_dns1;
    QLineEdit *m_dns2;
    QPushButton *m_apply;
};
