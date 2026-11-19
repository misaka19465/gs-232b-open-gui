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

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QUdpSocket>

/** @brief 同机 WSJT-X/JTDX 状态监听；DX 目标变化时在跟踪模式下转换方位。 */
class WsjtXController final : public QObject
{
    Q_OBJECT

public:
    /** @brief 创建关闭的接口，固定监听本机 UDP 2237。 */
    explicit WsjtXController(QObject *parent = nullptr);
    /** @brief 启停 UDP 监听；绑定失败返回 false，可由 errorString 查询原因。 */
    bool setEnabled(bool enabled);
    /** @return UDP 接口是否已启用。 */
    bool isEnabled() const;
    /** @return 最近一次监听或控制前校验失败的本地化错误。 */
    QString errorString() const;
    /** @brief 连接断开、透传或校准时传 false，期间只更新基线、不发送转向命令。 */
    void setControlAvailable(bool available);
    /** @brief 切换跟踪控制权限；模式变化时先消费排队状态，不重放旧目标。 */
    void setTrackingEnabled(bool enabled);
    /** @brief 更新本地本站网格；空值时回退到 WSJT-X/JTDX Status 中的网格。 */
    void setStationGrid(const QString &grid);
    /** @brief 更新设备实测方位；仅可控状态下的 0–450° 读数有效，有效期 3 秒。 */
    void setPosition(int mechanicalAzimuth);
    /** @brief 更新本地方位补偿，不启动旋转。 */
    void setAzimuthOffset(int degrees);
    /** @brief 更新重叠行程的选解偏好，不启动旋转。 */
    void setShortestPathEnabled(bool enabled);
    /** @brief 将后续呼号目标反转 180° 后选解。 */
    void setLongPathEnabled(bool enabled);

signals:
    /** @brief 请求发送 GS-232B 的 Mddd；宿主仍须检查设备可控状态。 */
    void commandRequested(const QString &command);
    /** @brief 监听失败或转向前缺少新鲜实测读数，供宿主显示。 */
    void errorOccurred(const QString &message);

private:
    void readDatagrams();
    void processSelectedTarget(const QString &callsign, const QString &grid);
    bool hasFreshPosition() const;

    QUdpSocket m_socket;
    QElapsedTimer m_positionAge;
    QString m_error;
    QString m_stationGrid;
    QString m_ownCallsign;
    QString m_ownGrid;
    QString m_dxCallsign;
    QString m_dxGrid;
    int m_position = 0;
    int m_offset = 0;
    bool m_haveStatusBaseline = false;
    bool m_controlAvailable = false;
    bool m_trackingEnabled = false;
    bool m_shortestPath = true;
    bool m_longPath = false;
    bool m_missingGridReported = false;
};
