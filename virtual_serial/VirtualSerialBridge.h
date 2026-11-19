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

#include "CrDelimitedBuffer.h"
#include "VirtualSerialEndpoint.h"

/** @brief 将虚拟串口字节流与 WebSocket 字节块按 CR 双向分行。 */
class VirtualSerialBridge final : public QObject
{
    Q_OBJECT

public:
    explicit VirtualSerialBridge(QObject *parent = nullptr);
    ~VirtualSerialBridge() override;

    /** @brief 启动平台端点；不创建或连接 WebSocket。 */
    bool start(const VirtualSerialOptions &options = {});
    /** @brief 停止端点并丢弃未完成行。 */
    void stop();
    /** @return 平台端点当前是否运行。 */
    bool isRunning() const;
    /** @return 外部串口软件应打开的路径或 COM 名称。 */
    QString clientPortName() const;
    /** @brief 输入任意大小的 WebSocket 消息块，完整 CR 行写入串口端点。
     *  @return 是否成功缓冲或转发；失败时调用方应关闭对应的 WebSocket 连接。
     */
    bool feedWebSocketBytes(const QByteArray &bytes);
    /** @brief 清除连接断开时可能遗留的半行数据。 */
    void clearPendingData();

signals:
    /** @brief 外部串口软件发来的完整行，包含结尾 CR，供调用方发送到 WebSocket。 */
    void webSocketLineReady(const QByteArray &line);
    /** @brief 虚拟端点已启动，参数可供外部串口软件打开。 */
    void started(const QString &clientPortName);
    /** @brief 虚拟端点已停止。 */
    void stopped();
    /** @brief 端点或转发错误。 */
    void errorOccurred(const QString &error);

private:
    void onSerialBytes(const QByteArray &bytes);

    VirtualSerialEndpoint m_endpoint;
    CrDelimitedBuffer m_serialInput;
    CrDelimitedBuffer m_webSocketInput;
};
