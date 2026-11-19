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

#include "WsjtXController.h"

#include "AzimuthScale.h"
#include "MaidenheadLocator.h"
#include "targeting/TargetBearing.h"
#include "WsjtXProtocol.h"

#include <QHostAddress>
#include <QNetworkDatagram>

WsjtXController::WsjtXController(QObject *parent) : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &WsjtXController::readDatagrams);
}

bool WsjtXController::setEnabled(bool enabled)
{
    if (enabled == isEnabled())
        return true;
    if (!enabled) {
        m_socket.close();
        m_haveStatusBaseline = false;
        m_ownCallsign.clear();
        m_ownGrid.clear();
        m_dxCallsign.clear();
        m_dxGrid.clear();
        m_missingGridReported = false;
        return true;
    }
    if (!m_socket.bind(QHostAddress::LocalHost, WsjtX::ControlPort, QUdpSocket::DontShareAddress)) {
        m_error = tr("Could not enable WSJT-X / JTDX integration: %1").arg(m_socket.errorString());
        emit errorOccurred(m_error);
        return false;
    }
    m_error.clear();
    return true;
}

bool WsjtXController::isEnabled() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

QString WsjtXController::errorString() const
{
    return m_error;
}

void WsjtXController::setControlAvailable(bool available)
{
    if (available != m_controlAvailable) {
        m_controlAvailable = false;
        if (isEnabled())
            readDatagrams();
    }
    m_controlAvailable = available;
    if (!available)
        m_positionAge.invalidate();
}

void WsjtXController::setTrackingEnabled(bool enabled)
{
    if (enabled == m_trackingEnabled)
        return;
    m_trackingEnabled = false;
    if (isEnabled())
        readDatagrams();
    m_trackingEnabled = enabled;
}

void WsjtXController::setStationGrid(const QString &grid)
{
    const QString normalized = grid.trimmed().toUpper();
    m_stationGrid = MaidenheadLocator::isValid(normalized) ? normalized : QString();
    if (!m_stationGrid.isEmpty() || !m_ownGrid.isEmpty())
        m_missingGridReported = false;
}

void WsjtXController::setPosition(int mechanicalAzimuth)
{
    if (!m_controlAvailable || mechanicalAzimuth < 0 || mechanicalAzimuth > AzimuthScale::Maximum) {
        m_positionAge.invalidate();
        return;
    }
    m_position = mechanicalAzimuth;
    m_positionAge.start();
}

void WsjtXController::setAzimuthOffset(int degrees)
{
    m_offset = degrees;
}

void WsjtXController::setShortestPathEnabled(bool enabled)
{
    m_shortestPath = enabled;
}

void WsjtXController::setLongPathEnabled(bool enabled)
{
    m_longPath = enabled;
}

bool WsjtXController::hasFreshPosition() const
{
    return m_controlAvailable && m_positionAge.isValid() && m_positionAge.elapsed() < 3000;
}

void WsjtXController::readDatagrams()
{
    while (isEnabled() && m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram(8193);
        if (!datagram.senderAddress().isLoopback())
            continue;
        if (const auto status = WsjtX::parseStatusDatagram(datagram.data())) {
            const bool targetChanged = !m_haveStatusBaseline ||
                (status->dxCallsign.compare(m_dxCallsign, Qt::CaseInsensitive) != 0 ||
                 status->dxGrid.compare(m_dxGrid, Qt::CaseInsensitive) != 0);
            m_haveStatusBaseline = true;
            m_ownCallsign = status->ownCallsign;
            m_ownGrid = status->ownGrid;
            m_dxCallsign = status->dxCallsign;
            m_dxGrid = status->dxGrid;
            if (!m_ownGrid.isEmpty())
                m_missingGridReported = false;
            if (targetChanged && !m_dxCallsign.isEmpty() &&
                (m_ownCallsign.isEmpty() ||
                 m_dxCallsign.compare(m_ownCallsign, Qt::CaseInsensitive) != 0) &&
                m_controlAvailable && m_trackingEnabled)
                processSelectedTarget(m_dxCallsign, m_dxGrid);
            continue;
        }
    }
}

void WsjtXController::processSelectedTarget(const QString &callsign, const QString &grid)
{
    const QString stationGrid = m_stationGrid.isEmpty() ? m_ownGrid : m_stationGrid;
    if (stationGrid.isEmpty()) {
        if (!m_missingGridReported) {
            m_error = tr("WSJT-X / JTDX target ignored: set your station grid first.");
            emit errorOccurred(m_error);
            m_missingGridReported = true;
        }
        return;
    }
    if (!hasFreshPosition()) {
        m_error = tr("WSJT-X / JTDX target ignored: no recent rotator position is available.");
        emit errorOccurred(m_error);
        return;
    }

    const QString target = grid.isEmpty() ? callsign : grid;
    const auto result = TargetBearing::calculate(stationGrid, target);
    if (!result.solution)
        return;

    const int bearing = m_longPath
        ? AzimuthScale::normalized(result.solution->roundedDegrees() + 180)
        : result.solution->roundedDegrees();
    const AzimuthScale scale(m_offset);
    const auto direct = scale.mechanical(bearing);
    const int mechanicalTarget = !m_shortestPath && direct
        ? *direct : scale.mechanicalBearing(bearing, m_shortestPath ? m_position : 0);
    emit commandRequested(QStringLiteral("M%1").arg(mechanicalTarget, 3, 10, QLatin1Char('0')));
}
