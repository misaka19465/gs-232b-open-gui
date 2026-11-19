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

#include <QJsonObject>
#include <QObject>
#include <QTimer>

/** @brief 轮询固件故障并执行显式清除；不持有传输或管理认证状态。 */
class FaultStatusMonitor final : public QObject
{
    Q_OBJECT

public:
    /** @brief 固件故障语义；未知响应与未知故障分别保留，避免误报正常。 */
    enum class Fault {
        Unknown, None, Sample, Output, Limit, FeedbackDirection, NoProgress,
        RunTimeout, SequenceLate, SequenceTarget, Other,
    };
    Q_ENUM(Fault)

    /** @brief 清除请求的结果；成功后仍重新读取实际故障状态。 */
    enum class ClearResult {
        Succeeded, Busy, NotOwner, NotAuthorized, FaultPersists, TimedOut, Failed,
    };
    Q_ENUM(ClearResult)

    /** @brief 创建停用状态的监视器，启用后立即读取设备状态。 */
    explicit FaultStatusMonitor(QObject *parent = nullptr);
    /** @brief 仅在已连接且非透传时启用；禁用会清除缓存状态及待处理请求。 */
    void setEnabled(bool enabled);
    /** @brief 校准交互期间暂停新查询与清除，仍处理已发请求的响应。 */
    void setPaused(bool paused);
    /** @brief 接收已解码管理响应；返回是否属于故障查询或清除。 */
    bool handleResponse(const QJsonObject &response);
    /** @brief 在已知故障且允许操作时发送清除，重复点击不会重复发送。 */
    void clearFault();

    /** @return 当前控制连接是否允许故障操作。 */
    bool isEnabled() const { return m_enabled; }
    /** @return 是否正在等待状态响应。 */
    bool isReading() const { return m_statusTimeout.isActive(); }
    /** @return 是否正在等待清除响应。 */
    bool isClearing() const { return m_clearTimeout.isActive(); }
    /** @return 是否已知有故障且当前可发出清除请求。 */
    bool canClear() const;
    /** @return 最近读取的故障；断连、查询失败或清除后待复核时为 Unknown。 */
    Fault fault() const { return m_fault; }
    /** @return bit0 方位、bit1 俯仰；零表示非特定轴故障。 */
    int axisMask() const { return m_axisMask; }

signals:
    /** @brief 宿主须通过当前控制连接发送，不得另取控制权或自动停机。 */
    void commandRequested(const QString &command);
    /** @brief 故障或操作可用性变化，调用方应重新读取当前状态。 */
    void stateChanged();
    /** @brief 清除操作响应或超时；实际故障以随后读取的状态为准。 */
    void clearFinished(FaultStatusMonitor::ClearResult result);

private:
    void requestStatus();
    void scheduleStatus();

    QTimer m_pollTimer;
    QTimer m_statusTimeout;
    QTimer m_clearTimeout;
    bool m_enabled = false;
    bool m_paused = false;
    Fault m_fault = Fault::Unknown;
    int m_axisMask = 0;
};
