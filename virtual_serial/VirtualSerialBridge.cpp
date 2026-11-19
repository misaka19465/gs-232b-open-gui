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

#include "VirtualSerialBridge.h"

VirtualSerialBridge::VirtualSerialBridge(QObject *parent)
    : QObject(parent)
{
    connect(&m_endpoint, &VirtualSerialEndpoint::started,
            this, &VirtualSerialBridge::started);
    connect(&m_endpoint, &VirtualSerialEndpoint::stopped,
            this, [this] {
                clearPendingData();
                emit stopped();
            });
    connect(&m_endpoint, &VirtualSerialEndpoint::bytesReceived,
            this, &VirtualSerialBridge::onSerialBytes);
    connect(&m_endpoint, &VirtualSerialEndpoint::errorOccurred,
            this, &VirtualSerialBridge::errorOccurred);
}

VirtualSerialBridge::~VirtualSerialBridge()
{
    stop();
}

bool VirtualSerialBridge::start(const VirtualSerialOptions &options)
{
    clearPendingData();
    return m_endpoint.start(options);
}

void VirtualSerialBridge::stop()
{
    clearPendingData();
    m_endpoint.stop();
}

bool VirtualSerialBridge::isRunning() const
{
    return m_endpoint.isRunning();
}

QString VirtualSerialBridge::clientPortName() const
{
    return m_endpoint.clientPortName();
}

bool VirtualSerialBridge::feedWebSocketBytes(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return true;
    if (!m_endpoint.isRunning()) {
        emit errorOccurred(tr("Virtual serial endpoint is not running"));
        return false;
    }

    const QList<QByteArray> lines = m_webSocketInput.append(bytes);
    for (const QByteArray &line : lines) {
        if (!m_endpoint.writeToClient(line))
            return false;
    }
    return true;
}

void VirtualSerialBridge::clearPendingData()
{
    m_serialInput.clear();
    m_webSocketInput.clear();
}

void VirtualSerialBridge::onSerialBytes(const QByteArray &bytes)
{
    const QList<QByteArray> lines = m_serialInput.append(bytes);
    for (const QByteArray &line : lines) {
        emit webSocketLineReady(line);
        if (!m_endpoint.isRunning())
            break;
    }
}
