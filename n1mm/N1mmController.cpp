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

#include "N1mmController.h"
#include "N1mmProtocol.h"
#include "AzimuthScale.h"

#include <QHostAddress>
#include <QNetworkDatagram>

N1mmController::N1mmController(QObject *parent) : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &N1mmController::readDatagrams);
}

bool N1mmController::setEnabled(bool enabled)
{
    if (enabled == isEnabled())
        return true;
    if (!enabled) {
        m_socket.close();
        return true;
    }
    // NOTE: 首版限定同机；独占绑定可发现原配程序或另一 GUI 实例仍占用控制端口。
    if (!m_socket.bind(QHostAddress::LocalHost, N1mm::ControlPort, QUdpSocket::DontShareAddress)) {
        m_error = tr("Could not enable N1MM Logger+ integration: %1").arg(m_socket.errorString());
        emit errorOccurred(m_error);
        return false;
    }
    m_error.clear();
    reportPosition();
    return true;
}

bool N1mmController::isEnabled() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

QString N1mmController::errorString() const
{
    return m_error;
}

bool N1mmController::setRotorName(const QString &name)
{
    if (!N1mm::isValidRotorName(name))
        return false;
    m_rotorName = name.trimmed();
    reportPosition();
    return true;
}

QString N1mmController::rotorName() const
{
    return m_rotorName;
}

void N1mmController::setControlAvailable(bool available)
{
    if (available != m_controlAvailable) {
        // NOTE: 可控状态切换时丢弃已排队数据，防止断线或校准期间的命令在恢复后执行。
        while (isEnabled() && m_socket.hasPendingDatagrams())
            m_socket.receiveDatagram(0);
    }
    m_controlAvailable = available;
    if (!available)
        m_positionAge.invalidate();
}

void N1mmController::setPosition(int mechanicalAzimuth)
{
    if (!m_controlAvailable || mechanicalAzimuth < 0 || mechanicalAzimuth > AzimuthScale::Maximum) {
        m_positionAge.invalidate();
        return;
    }
    m_position = mechanicalAzimuth;
    m_positionAge.start();
    reportPosition();
}

void N1mmController::setTrackingEnabled(bool enabled)
{
    if (enabled == m_trackingEnabled)
        return;
    // NOTE: 手动期间的控制报文不得在切回跟踪后重放；模式切换不作废实测读数。
    while (isEnabled() && m_socket.hasPendingDatagrams())
        m_socket.receiveDatagram(0);
    m_trackingEnabled = enabled;
}

void N1mmController::setAzimuthOffset(int degrees)
{
    m_offset = degrees;
    reportPosition();
}

void N1mmController::setShortestPathEnabled(bool enabled)
{
    m_shortestPath = enabled;
}

bool N1mmController::hasFreshPosition() const
{
    return m_controlAvailable && m_positionAge.isValid() && m_positionAge.elapsed() < 3000;
}

void N1mmController::readDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram(8193);
        if (!datagram.senderAddress().isLoopback())
            continue;
        const auto command = N1mm::parseCommand(datagram.data());
        if (!command || !command->rotors.contains(m_rotorName) || !m_controlAvailable ||
            !m_trackingEnabled)
            continue;
        if (command->kind == N1mm::Command::Kind::Stop) {
            // NOTE: N1MM 只控制方位；A 停止方位轴，不干扰 GUI 独立控制的仰角轴。
            emit commandRequested(QStringLiteral("A"));
        } else if (hasFreshPosition()) {
            const int target = N1mm::mechanicalTarget(*command, m_position, m_offset, m_shortestPath);
            emit commandRequested(QStringLiteral("M%1").arg(target, 3, 10, QLatin1Char('0')));
        } else {
            emit errorOccurred(tr("N1MM turn command ignored: no recent rotator position is available."));
        }
    }
}

void N1mmController::reportPosition()
{
    if (!isEnabled() || !hasFreshPosition())
        return;
    const int heading = AzimuthScale::normalized(AzimuthScale(m_offset).corrected(m_position));
    const QByteArray packet = (m_rotorName + QLatin1Char('@') + QString::number(heading)).toUtf8();
    if (m_socket.writeDatagram(packet, QHostAddress::LocalHost, N1mm::ReportingPort) != packet.size()) {
        m_error = tr("Could not report the rotator position to N1MM Logger+: %1")
                      .arg(m_socket.errorString());
        emit errorOccurred(m_error);
    }
}
