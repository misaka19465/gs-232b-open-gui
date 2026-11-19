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

#include "CommunicationProtocol.h"
#include "virtual_serial/VirtualSerialEndpoint.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QRadioButton;

/** @brief 串口、WebSocket 和虚拟串口连接对话框。 */
class ConnectionDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建断开状态的连接对话框；连接成功后自动关闭。 */
    explicit ConnectionDialog(QWidget *parent = nullptr);
    /** @return 是否已经收到传输后端的连接成功状态。 */
    bool isConnected() const;
    /** @return 是否正在等待传输后端完成连接。 */
    bool isConnecting() const;
    /** @return 当前是否选择 WebSocket 传输。 */
    bool isNetworkMode() const;
    /** @return 是否启用 WebSocket 到虚拟串口的透传模式。 */
    bool isVirtualComEnabled() const;
    /** @return 当前表单对应的串口或 WebSocket 连接参数。 */
    Communication::ConnectionOptions connectionOptions(
        Communication::WebSocketEndpoint endpoint = Communication::WebSocketEndpoint::Gs232b) const;
    /** @return Windows 已配置的虚拟 COM 对；POSIX 使用自动分配的 PTY。 */
    VirtualSerialOptions virtualSerialOptions() const;
    /** @brief 由传输后端更新实际连接状态；点击连接按钮不直接改变此状态。 */
    void setConnected(bool connected);
    /** @brief 在异步 WebSocket 握手期间锁定连接表单。 */
    void setConnecting(bool connecting);
    /** @brief 在表单内显示最近一次连接错误，下次连接时清除。 */
    void setError(const QString &error);

signals:
    /** @brief 传输模式、透传选项或实际连接状态变更。 */
    void stateChanged();
    /** @brief 用户请求按当前表单建立真实传输连接。 */
    void connectRequested();
    /** @brief 用户请求关闭真实传输连接。 */
    void disconnectRequested();

private:
    void updateControls();
    void reportSettingsResult(bool saved);

    bool m_connected = false;
    QRadioButton *m_serialMode;
    QRadioButton *m_networkMode;
    QComboBox *m_serialPort;
    QComboBox *m_baudRate;
    QLineEdit *m_address;
    QCheckBox *m_virtualCom;
#ifdef Q_OS_WIN
    QWidget *m_virtualPortOptions;
    QComboBox *m_virtualApplicationPort;
    QComboBox *m_virtualClientPort;
#endif
    QPushButton *m_connect;
    QLabel *m_status;
    QString m_error;
    bool m_connecting = false;
};
