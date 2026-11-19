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

#include <QPointF>
#include <QString>

#include <optional>

namespace MaidenheadLocator {

/** @brief 校验 4、6、8 位 Maidenhead 网格，忽略大小写和首尾空白。 */
bool isValid(const QString &grid);

/** @return 网格地理中心（x 为经度、y 为纬度，单位为度）；非法网格返回空值。 */
std::optional<QPointF> center(const QString &grid);

} // namespace MaidenheadLocator
