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

#include <QByteArray>
#include <QString>

#include <optional>

namespace WsjtX {

inline constexpr quint16 ControlPort = 2237;

/** @brief WSJT-X/JTDX 共享 Status 消息中的本站和当前 DX 目标字段。 */
struct Status {
    QString ownCallsign;
    QString ownGrid;
    QString dxCallsign;
    QString dxGrid;
};

/** @brief 解析 WSJT-X/JTDX schema 2 或 3 共同字段；其他类型与版本返回空值。 */
std::optional<Status> parseStatusDatagram(const QByteArray &datagram);

} // namespace WsjtX
