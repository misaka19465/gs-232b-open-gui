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

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QUdpSocket>

/** @brief 同机单旋转器的 N1MM UDP 接口；仅在设备可控且有新鲜读数时执行方位定位。 */
class N1mmController final : public QObject
{
    Q_OBJECT

public:
    /** @brief 创建关闭的接口；监听本机 12040，向本机 13010 回传方位。 */
    explicit N1mmController(QObject *parent = nullptr);
    /** @brief 启停 UDP 监听；绑定失败返回 false，可由 errorString 查询原因。 */
    bool setEnabled(bool enabled);
    /** @return UDP 控制接口是否已启用。 */
    bool isEnabled() const;
    /** @return 最近一次监听或发送失败的本地化错误。 */
    QString errorString() const;
    /** @brief 设置合法旋转器名，自动去除首尾空白；不合法时返回 false 并保留原名。 */
    bool setRotorName(const QString &name);
    /** @return 当前精确匹配控制报文并用于位置反馈的旋转器名。 */
    QString rotorName() const;
    /** @brief 连接断开、透传或校准时传 false，同时作废缓存位置，丢弃期间的命令。 */
    void setControlAvailable(bool available);
    /** @brief 切换跟踪控制权限，默认手动；丢弃切换前排队命令，保持实测位置反馈。 */
    void setTrackingEnabled(bool enabled);
    /** @brief 更新设备实测方位；有效读数可使用 3 秒，关闭接入时同样更新缓存。 */
    void setPosition(int mechanicalAzimuth);
    /** @brief 更新本地设备补偿并重报新鲜位置，不启动旋转。 */
    void setAzimuthOffset(int degrees);
    /** @brief 更新重叠行程的选解偏好，不启动旋转。 */
    void setShortestPathEnabled(bool enabled);

signals:
    /** @brief 请求发送 GS-232B 的 Mddd 或 A；宿主仍须检查真实连接状态。 */
    void commandRequested(const QString &command);
    /** @brief 接口错误或定位前缺失新鲜读数；供宿主显示，不包含设备命令。 */
    void errorOccurred(const QString &message);

private:
    void readDatagrams();
    void reportPosition();
    bool hasFreshPosition() const;

    QUdpSocket m_socket;
    QElapsedTimer m_positionAge;
    QString m_rotorName = QStringLiteral("GS232B");
    QString m_error;
    int m_position = 0;
    int m_offset = 0;
    bool m_controlAvailable = false;
    bool m_trackingEnabled = false;
    bool m_shortestPath = true;
};
