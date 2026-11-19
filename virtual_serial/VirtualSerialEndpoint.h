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
#include <QObject>
#include <QString>

#include <memory>

/** @brief 配置由当前进程持有的一端和外部客户端使用的一端。 */
struct VirtualSerialOptions
{
    /** @brief Windows 配对驱动中由当前进程打开的 COM 端口。 */
    QString applicationPort;
    /** @brief Windows 配对驱动中留给外部客户端的 COM 端口。 */
    QString clientPort;
    /** @brief Windows 当前进程端口的线路速率；POSIX PTY 不模拟线路时钟。 */
    int baudRate = 9600;
};

/** @brief 提供平台原生虚拟串口端点，POSIX 使用 PTY，Windows 使用已配置的 COM 对。 */
class VirtualSerialEndpoint final : public QObject
{
    Q_OBJECT

public:
    explicit VirtualSerialEndpoint(QObject *parent = nullptr);
    ~VirtualSerialEndpoint() override;

    /** @brief 创建端点；Windows 需要先安装并配置配对虚拟串口驱动。 */
    bool start(const VirtualSerialOptions &options = {});
    /** @brief 关闭端点并释放平台资源。 */
    void stop();
    /** @return 端点当前是否运行。 */
    bool isRunning() const;
    /** @return 外部串口软件应打开的路径或 COM 名称。 */
    QString clientPortName() const;
    /** @brief 将 WebSocket 侧收到的完整 CR 行写入外部串口端点。 */
    bool writeToClient(const QByteArray &bytes);

signals:
    /** @brief 端点启动完成，参数可供外部串口软件打开。 */
    void started(const QString &clientPortName);
    /** @brief 端点停止或 Windows 配对端断开。 */
    void stopped();
    /** @brief 从外部串口软件收到的原始字节块。 */
    void bytesReceived(const QByteArray &bytes);
    /** @brief 平台端点错误。 */
    void errorOccurred(const QString &error);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    QString m_clientPortName;
    bool m_running = false;
};
