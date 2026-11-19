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

#include "FaultStatusMonitor.h"

namespace {

FaultStatusMonitor::Fault decodeFault(const QJsonValue &value)
{
    using Fault = FaultStatusMonitor::Fault;
    const int code = value.toInt(-1);
    if (!value.isDouble() || code < 0 || value.toDouble() != code)
        return Fault::Unknown;

    // NOTE: 数值对应下位机 include/gs232b/types.h；新故障仍显示异常，不能回退为正常。
    switch (code) {
    case 0: return Fault::None;
    case 1: return Fault::Sample;
    case 2: return Fault::Output;
    case 3: return Fault::Limit;
    case 4: return Fault::FeedbackDirection;
    case 5: return Fault::NoProgress;
    case 6: return Fault::RunTimeout;
    case 7: return Fault::SequenceLate;
    case 8: return Fault::SequenceTarget;
    default: return Fault::Other;
    }
}

FaultStatusMonitor::ClearResult clearResult(const QJsonObject &response)
{
    using Result = FaultStatusMonitor::ClearResult;
    if (response.value(QStringLiteral("ok")).toBool() &&
        response.value(QStringLiteral("data")).toObject()
            .value(QStringLiteral("cleared")).toBool())
        return Result::Succeeded;

    const QString error = response.value(QStringLiteral("error")).toString();
    if (error == QStringLiteral("BUSY"))
        return Result::Busy;
    if (error == QStringLiteral("NOT_OWNER"))
        return Result::NotOwner;
    if (error == QStringLiteral("NOT_AUTHORIZED"))
        return Result::NotAuthorized;
    if (error == QStringLiteral("NOT_READY") || error == QStringLiteral("HARDWARE_FAULT") ||
        error == QStringLiteral("IO"))
        return Result::FaultPersists;
    if (error == QStringLiteral("TIMEOUT"))
        return Result::TimedOut;
    return Result::Failed;
}

} // namespace

FaultStatusMonitor::FaultStatusMonitor(QObject *parent) : QObject(parent)
{
    // NOTE: Qt 5 需要显式注册信号枚举，确保队列连接和信号观察器能读取清除结果。
    qRegisterMetaType<ClearResult>("FaultStatusMonitor::ClearResult");
    m_pollTimer.setSingleShot(true);
    m_pollTimer.setInterval(2000);
    m_statusTimeout.setSingleShot(true);
    m_clearTimeout.setSingleShot(true);
    // NOTE: 完整状态在 1200 波特率下需十余秒；等待响应后再排下次查询，避免积压。
    m_statusTimeout.setInterval(20000);
    m_clearTimeout.setInterval(20000);
    connect(&m_pollTimer, &QTimer::timeout, this, &FaultStatusMonitor::requestStatus);
    connect(&m_statusTimeout, &QTimer::timeout, this, [this] {
        m_fault = Fault::Unknown;
        emit stateChanged();
        scheduleStatus();
    });
    connect(&m_clearTimeout, &QTimer::timeout, this, [this] {
        m_fault = Fault::Unknown;
        emit stateChanged();
        emit clearFinished(ClearResult::TimedOut);
        requestStatus();
    });
}

void FaultStatusMonitor::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!enabled)
        m_paused = false;
    m_pollTimer.stop();
    m_statusTimeout.stop();
    m_clearTimeout.stop();
    m_fault = Fault::Unknown;
    m_axisMask = 0;
    emit stateChanged();
    if (enabled)
        requestStatus();
}

void FaultStatusMonitor::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    m_pollTimer.stop();
    emit stateChanged();
    if (!paused)
        requestStatus();
}

bool FaultStatusMonitor::canClear() const
{
    return m_enabled && !m_paused && !isClearing() &&
           m_fault != Fault::Unknown && m_fault != Fault::None;
}

void FaultStatusMonitor::requestStatus()
{
    if (!m_enabled || m_paused || isReading() || isClearing())
        return;
    m_pollTimer.stop();
    m_statusTimeout.start();
    emit stateChanged();
    emit commandRequested(QStringLiteral("!MOTION STATUS"));
}

void FaultStatusMonitor::scheduleStatus()
{
    if (m_enabled && !m_paused && !isClearing())
        m_pollTimer.start();
}

void FaultStatusMonitor::clearFault()
{
    if (!canClear())
        return;
    m_pollTimer.stop();
    m_clearTimeout.start();
    emit stateChanged();
    emit commandRequested(QStringLiteral("!FAULT CLEAR"));
}

bool FaultStatusMonitor::handleResponse(const QJsonObject &response)
{
    const QString command = response.value(QStringLiteral("cmd")).toString().toUpper();
    if (command != QStringLiteral("MOTION STATUS") && command != QStringLiteral("FAULT CLEAR"))
        return false;
    if (!m_enabled)
        return true;

    if (command == QStringLiteral("MOTION STATUS")) {
        m_statusTimeout.stop();
        const auto fault = response.value(QStringLiteral("data")).toObject()
                               .value(QStringLiteral("fault")).toObject();
        m_fault = response.value(QStringLiteral("ok")).toBool()
                      ? decodeFault(fault.value(QStringLiteral("code"))) : Fault::Unknown;
        m_axisMask = fault.value(QStringLiteral("axis_mask")).toInt();
        if (m_axisMask < 0 || m_axisMask > 3)
            m_axisMask = 0;
        emit stateChanged();
        scheduleStatus();
    } else {
        m_clearTimeout.stop();
        const auto result = clearResult(response);
        // NOTE: 清除确认不是状态快照；重读前不能把本地缓存直接改成“无报错”。
        m_fault = Fault::Unknown;
        emit stateChanged();
        emit clearFinished(result);
        requestStatus();
    }
    return true;
}
