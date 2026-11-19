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

#include "WsjtXProtocol.h"

#include "MaidenheadLocator.h"

#include <limits>

namespace {

constexpr quint32 Magic = 0xadbccbda;
constexpr quint32 StatusMessageType = 1;
constexpr quint32 MinimumSchema = 2;
constexpr quint32 MaximumSchema = 3;
constexpr qsizetype MaximumDatagramSize = 8192;
constexpr qsizetype MaximumInstanceIdSize = 128;
constexpr qsizetype MaximumModeSize = 32;
constexpr qsizetype MaximumMessageSize = 1024;

class Reader final
{
public:
    explicit Reader(const QByteArray &data) : m_data(data) {}

    bool readU8(quint8 &value)
    {
        if (remaining() < 1)
            return false;
        value = quint8(m_data.at(m_offset++));
        return true;
    }

    bool readU32(quint32 &value)
    {
        if (remaining() < 4)
            return false;
        value = 0;
        for (int index = 0; index < 4; ++index)
            value = (value << 8) | quint8(m_data.at(m_offset++));
        return true;
    }

    bool readU64(quint64 &value)
    {
        if (remaining() < 8)
            return false;
        value = 0;
        for (int index = 0; index < 8; ++index)
            value = (value << 8) | quint8(m_data.at(m_offset++));
        return true;
    }

    bool readBoolean(bool &value)
    {
        quint8 byte = 0;
        if (!readU8(byte) || byte > 1)
            return false;
        value = byte != 0;
        return true;
    }

    bool readByteArray(QByteArray &value, qsizetype maximumSize, bool allowEmpty = true)
    {
        quint32 size = 0;
        if (!readU32(size))
            return false;
        if (size == std::numeric_limits<quint32>::max() && allowEmpty) {
            value.clear();
            return true;
        }
        if (size > quint32(maximumSize) || size > quint32(remaining()) ||
            (!allowEmpty && size == 0))
            return false;
        value = QByteArray(m_data.constData() + m_offset, int(size));
        m_offset += int(size);
        return true;
    }

    qsizetype remaining() const { return m_data.size() - m_offset; }

private:
    const QByteArray &m_data;
    qsizetype m_offset = 0;
};

bool readHeader(Reader &reader, quint32 expectedType, QByteArray &instanceId)
{
    quint32 magic = 0;
    quint32 schema = 0;
    quint32 messageType = 0;
    if (!reader.readU32(magic) || !reader.readU32(schema) || !reader.readU32(messageType) ||
        !reader.readByteArray(instanceId, MaximumInstanceIdSize, false))
        return false;
    return magic == Magic && schema >= MinimumSchema && schema <= MaximumSchema &&
           messageType == expectedType;
}

QString normalizedGrid(const QByteArray &bytes)
{
    const QString grid = QString::fromUtf8(bytes).trimmed().toUpper();
    return MaidenheadLocator::isValid(grid) ? grid : QString();
}

} // namespace

std::optional<WsjtX::Status> WsjtX::parseStatusDatagram(const QByteArray &datagram)
{
    if (datagram.isEmpty() || datagram.size() > MaximumDatagramSize)
        return std::nullopt;
    Reader reader(datagram);
    QByteArray instanceId;
    if (!readHeader(reader, StatusMessageType, instanceId))
        return std::nullopt;

    quint64 dialFrequency = 0;
    QByteArray mode;
    QByteArray dxCall;
    QByteArray report;
    QByteArray txMode;
    bool txEnabled = false;
    bool transmitting = false;
    bool decoding = false;
    quint32 rxFrequency = 0;
    quint32 txFrequency = 0;
    QByteArray ownCallsign;
    QByteArray ownGrid;
    QByteArray dxGrid;
    if (!reader.readU64(dialFrequency) || !reader.readByteArray(mode, MaximumModeSize) ||
        !reader.readByteArray(dxCall, MaximumMessageSize) ||
        !reader.readByteArray(report, MaximumMessageSize) ||
        !reader.readByteArray(txMode, MaximumModeSize) || !reader.readBoolean(txEnabled) ||
        !reader.readBoolean(transmitting) || !reader.readBoolean(decoding) ||
        !reader.readU32(rxFrequency) || !reader.readU32(txFrequency) ||
        !reader.readByteArray(ownCallsign, MaximumMessageSize) ||
        !reader.readByteArray(ownGrid, MaximumMessageSize) ||
        !reader.readByteArray(dxGrid, MaximumMessageSize))
        return std::nullopt;
    Q_UNUSED(dialFrequency)
    Q_UNUSED(report)
    Q_UNUSED(txEnabled)
    Q_UNUSED(transmitting)
    Q_UNUSED(decoding)
    Q_UNUSED(rxFrequency)
    Q_UNUSED(txFrequency)

    return Status{QString::fromUtf8(ownCallsign).trimmed().toUpper(),
                  normalizedGrid(ownGrid), QString::fromUtf8(dxCall).trimmed().toUpper(),
                  normalizedGrid(dxGrid)};
}
