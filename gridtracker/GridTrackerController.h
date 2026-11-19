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

/** @brief 同机 GridTracker 的 PstRotator UDP 桥接，执行定位并应答新鲜实测位置查询。 */
class GridTrackerController final : public QObject
{
    Q_OBJECT

public:
    /** @brief 创建关闭的接口，固定监听 127.0.0.1:12000，查询回复发往 127.0.0.1:12001。 */
    explicit GridTrackerController(QObject *parent = nullptr);
    /** @brief 启停独占 UDP 监听；失败返回 false，并可由 errorString 查询原因。 */
    bool setEnabled(bool enabled);
    /** @return UDP 接口是否已启用。 */
    bool isEnabled() const;
    /** @return 最近一次监听或回复失败的本地化错误。 */
    QString errorString() const;
    /** @brief 断线、透传和校准时传 false，作废两轴读数并丢弃期间命令。 */
    void setControlAvailable(bool available);
    /** @brief 切换跟踪控制权限，默认手动；丢弃排队控制，继续应答查询且保留实测缓存。 */
    void setTrackingEnabled(bool enabled);
    /** @brief 更新 0–450° 实测机械方位，有效期 3 秒；不可控或越界时作废。 */
    void setAzimuthPosition(int mechanicalAzimuth);
    /** @brief 更新 0–180° 实测仰角，有效期 3 秒；不可控或越界时作废。 */
    void setElevationPosition(int elevation);
    /** @brief 更新方位补偿，不启动旋转；位置查询同样使用该补偿。 */
    void setAzimuthOffset(int degrees);
    /** @brief 更新重叠行程的选解偏好，不启动旋转。 */
    void setShortestPathEnabled(bool enabled);
    /** @brief 将后续方位目标反转 180° 后选解，不影响仰角或实测位置反馈。 */
    void setLongPathEnabled(bool enabled);

signals:
    /** @brief 请求发送 GS-232B 的 Mddd 或 Wddd eee；宿主仍须检查设备可控状态。 */
    void commandRequested(const QString &command);
    /** @brief 监听、回复失败或定位缺少新鲜实测方位，供宿主显示。 */
    void errorOccurred(const QString &message);

private:
    void readDatagrams();
    void reportPosition(bool azimuth);
    bool hasFreshPosition(const QElapsedTimer &age) const;

    QUdpSocket m_socket;
    QElapsedTimer m_azimuthAge;
    QElapsedTimer m_elevationAge;
    QString m_error;
    int m_azimuth = 0;
    int m_elevation = 0;
    int m_offset = 0;
    bool m_controlAvailable = false;
    bool m_trackingEnabled = false;
    bool m_shortestPath = true;
    bool m_longPath = false;
};
