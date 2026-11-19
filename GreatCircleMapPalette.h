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

#include <QImage>
#include <QPalette>

namespace GreatCircleMapPalette {

/** @brief 使用主题活动配色重绘 NS6T 底图，保留投影、线条细节和透明边缘。 */
inline QImage recolor(const QImage &source, const QPalette &palette)
{
    const QRgb land = palette.color(QPalette::Active, QPalette::Base).rgb();
    const QRgb accent = palette.color(QPalette::Active, QPalette::Highlight).rgb();
    const QRgb ink = palette.color(QPalette::Active, QPalette::Text).rgb();
    const auto tint = [](int base, int highlight) { return qRound(base * 0.65 + highlight * 0.35); };
    const QRgb water = qRgb(tint(qRed(land), qRed(accent)), tint(qGreen(land), qGreen(accent)),
                            tint(qBlue(land), qBlue(accent)));

    // NOTE: 源图由白色陆地、天蓝色海洋和黑色线条混合而成。
    // 用蓝红差分离海洋比例，重建混色以保留海岸和网格线的抗锯齿。
    constexpr QRgb sourceWater = qRgb(135, 206, 235);
    QImage image = source.convertToFormat(QImage::Format_ARGB32);
    for (int row = 0; row < image.height(); ++row) {
        auto *pixels = reinterpret_cast<QRgb *>(image.scanLine(row));
        for (int column = 0; column < image.width(); ++column) {
            QRgb &pixel = pixels[column];
            if (qAlpha(pixel) == 0)
                continue;

            const qreal waterWeight = qBound(qreal(0), (qBlue(pixel) - qRed(pixel)) /
                qreal(qBlue(sourceWater) - qRed(sourceWater)), qreal(1));
            const qreal landWeight = qBound(qreal(0), (qRed(pixel) - qRed(sourceWater) * waterWeight) /
                qreal(255), 1 - waterWeight);
            const qreal inkWeight = 1 - waterWeight - landWeight;
            const auto channel = [=](int landChannel, int waterChannel, int inkChannel) {
                return qRound(landChannel * landWeight + waterChannel * waterWeight + inkChannel * inkWeight);
            };
            pixel = qRgba(channel(qRed(land), qRed(water), qRed(ink)),
                          channel(qGreen(land), qGreen(water), qGreen(ink)),
                          channel(qBlue(land), qBlue(water), qBlue(ink)), qAlpha(pixel));
        }
    }
    image.convertTo(QImage::Format_ARGB32_Premultiplied);
    return image;
}

} // namespace GreatCircleMapPalette
