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

#include <QByteArray>
#include <QGroupBox>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

/** @brief 显示真实收发流并提供手动命令输入的控制台。 */
class ConsolePanel final : public QGroupBox
{
    Q_OBJECT

public:
    /** @brief 创建空白、可选中文字的控制台。 */
    explicit ConsolePanel(QWidget *parent = nullptr);
    /** @brief 统一切换需要发送数据的输入框与按钮。 */
    void setCommandEnabled(bool enabled);

public slots:
    /** @brief 将收到的真实字节追加到终端。 */
    void appendReceived(const QByteArray &bytes);
    /** @brief 将底层接受发送的字节追加到终端；敏感命令由调用方传入脱敏副本。 */
    void appendSent(const QByteArray &bytes);
    /** @brief 将传输错误追加到终端，不伪装成设备响应。 */
    void appendError(const QString &error);
    /** @brief 将外部工具输出追加到终端，使用独立标记避免冒充设备数据。 */
    void appendProcessOutput(const QByteArray &bytes);
    /** @brief 将应用状态消息追加到终端，不标记成设备收发或错误。 */
    void appendNotice(const QString &message);

signals:
    /** @brief 用户请求发送一行命令，具体编码由传输协议模块负责。 */
    void commandRequested(const QString &command);

private:
    void requestCommand();
    void appendTraffic(const QByteArray &bytes, bool outgoing);

    QPlainTextEdit *m_output;
    QLineEdit *m_input;
    QPushButton *m_send;
    bool m_lineStart = true;
};
