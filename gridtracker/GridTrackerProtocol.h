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

#include <QByteArray>
#include <QList>

#include <optional>

namespace GridTracker {

inline constexpr quint16 ControlPort = 12000;
inline constexpr quint16 ReportingPort = ControlPort + 1;

/** @brief GridTracker 的 PstRotator 请求；目标为圆周方位和绝对仰角，不含本地补偿。 */
struct Request {
    enum class Kind { Turn, QueryAzimuth, QueryElevation };
    Kind kind = Kind::Turn;
    std::optional<int> azimuth;
    std::optional<int> elevation;
};

/**
 * @brief 原子解析整个 UDP 数据报，支持多个连续 PST 查询；任一部分非法则全部拒绝。
 * @return 仅接受 AZ?/EL? 或 AZIMUTH（0–360°）/ELEVATION（0–180°），小数四舍五入到整数度。
 */
std::optional<QList<Request>> parseDatagram(const QByteArray &datagram);

} // namespace GridTracker
