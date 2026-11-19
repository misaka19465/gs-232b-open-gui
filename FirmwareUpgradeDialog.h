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

class QComboBox;
class QLineEdit;
class QPushButton;

/** @brief 选择下载串口和固件并进行二次确认的升级窗口原型。 */
class FirmwareUpgradeDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建独立升级窗口；不会枚举设备或执行烧录。 */
    explicit FirmwareUpgradeDialog(QWidget *parent = nullptr);

signals:
    /** @brief 用户确认烧录后请求主窗口执行下载工具。 */
    void flashRequested(const QString &port, const QString &file);

private:
    void selectFirmware();
    void confirmFlash();

    QComboBox *m_port;
    QLineEdit *m_file;
    QPushButton *m_flash;
};
