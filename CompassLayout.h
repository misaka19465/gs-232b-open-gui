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

#include <QHBoxLayout>

/** @brief 将罗盘限制在左侧正方形内，其余宽度交给右侧控制区。 */
class CompassLayout final : public QHBoxLayout
{
public:
    /** @brief 接管两个控件的布局；正方形边长同时受高度和控制区最小宽度约束。 */
    CompassLayout(QWidget *compass, QWidget *controls, QWidget *parent)
        : QHBoxLayout(parent)
    {
        setContentsMargins(0, 0, 0, 0);
        setSpacing(10);
        addWidget(compass);
        addWidget(controls, 1);
    }

    /** @brief 先更新标准布局状态，再按可用高度划分正方形与控制区。 */
    void setGeometry(const QRect &rect) override
    {
        QHBoxLayout::setGeometry(rect);
        const QRect bounds = contentsRect();
        const int gap = spacing();
        const int side = qMin(bounds.height(),
                              qMax(0, bounds.width() - gap - itemAt(1)->minimumSize().width()));
        itemAt(0)->setGeometry(QRect(bounds.x(), bounds.y() + (bounds.height() - side) / 2,
                                     side, side));
        itemAt(1)->setGeometry(QRect(bounds.x() + side + gap, bounds.y(),
                                     qMax(0, bounds.width() - side - gap), bounds.height()));
    }
};
