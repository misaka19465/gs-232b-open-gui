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

#include "TargetBearing.h"
#include "DxccDatabase.h"
#include "MaidenheadLocator.h"

#include <QtMath>

#include <cmath>

int TargetBearing::Solution::roundedDegrees() const
{
    return qRound(degrees) % 360;
}

std::optional<TargetBearing::Target> TargetBearing::resolve(const QString &target)
{
    const QString normalized = target.trimmed().toUpper();
    if (const auto center = MaidenheadLocator::center(target))
        return Target{TargetType::Grid, normalized, *center, {}, {}};
    if (const auto entity = DxccDatabase::instance().find(target))
        return Target{TargetType::Dxcc, normalized, entity->coordinates,
                      entity->name, entity->primaryPrefix};
    return std::nullopt;
}

std::optional<double> TargetBearing::initialBearing(const QPointF &station, const QPointF &target)
{
    for (const QPointF &point : {station, target}) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()) ||
            std::abs(point.x()) > 180 || std::abs(point.y()) > 90)
            return std::nullopt;
    }
    if (std::abs(station.y()) == 90)
        return std::nullopt;
    const double latitude = qDegreesToRadians(station.y());
    const double targetLatitude = qDegreesToRadians(target.y());
    const double delta = qDegreesToRadians(target.x() - station.x());
    const double east = std::sin(delta) * std::cos(targetLatitude);
    const double north = std::cos(latitude) * std::sin(targetLatitude) -
                         std::sin(latitude) * std::cos(targetLatitude) * std::cos(delta);
    // NOTE: 同点与对跖点均无唯一初始方位，不能把 atan2(0, 0) 当作正北旋转。
    if (std::hypot(east, north) < 1e-12)
        return std::nullopt;
    return std::fmod(qRadiansToDegrees(std::atan2(east, north)) + 360.0, 360.0);
}

TargetBearing::Result TargetBearing::calculate(const QString &stationGrid, const QString &target)
{
    const auto station = MaidenheadLocator::center(stationGrid);
    if (!station)
        return {Error::InvalidStationGrid, std::nullopt};
    const auto resolved = resolve(target);
    if (!resolved)
        return {Error::UnknownTarget, std::nullopt};
    const auto bearing = initialBearing(*station, resolved->coordinates);
    if (!bearing)
        return {Error::UndefinedBearing, std::nullopt};
    return {Error::None, Solution{*resolved, *bearing}};
}
