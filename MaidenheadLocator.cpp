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

#include "MaidenheadLocator.h"

#include <QRegularExpression>

bool MaidenheadLocator::isValid(const QString &grid)
{
    // NOTE: Maidenhead 前两位限 A–R，子网格限 A–X，后续数字对决定更高精度。
    static const QRegularExpression pattern(
        QStringLiteral("\\A[A-Ra-r]{2}[0-9]{2}(?:[A-Xa-x]{2}(?:[0-9]{2})?)?\\z"));
    return pattern.match(grid.trimmed()).hasMatch();
}

std::optional<QPointF> MaidenheadLocator::center(const QString &grid)
{
    if (!isValid(grid))
        return std::nullopt;

    const QString locator = grid.trimmed().toUpper();
    double longitude = -180 + (locator[0].unicode() - 'A') * 20 + locator[2].digitValue() * 2;
    double latitude = -90 + (locator[1].unicode() - 'A') * 10 + locator[3].digitValue();
    double width = 2;
    double height = 1;
    if (locator.size() >= 6) {
        width /= 24;
        height /= 24;
        longitude += (locator[4].unicode() - 'A') * width;
        latitude += (locator[5].unicode() - 'A') * height;
    }
    if (locator.size() == 8) {
        width /= 10;
        height /= 10;
        longitude += locator[6].digitValue() * width;
        latitude += locator[7].digitValue() * height;
    }
    return QPointF(longitude + width / 2, latitude + height / 2);
}
