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

#include <QHash>
#include <QPointF>
#include <QString>

#include <optional>

/** @brief 基于内置 CTY.DAT 的只读 DXCC 查询，不依赖界面、配置或网络。 */
class DxccDatabase final
{
public:
    /** @brief DXCC 代表位置；coordinates 的 x 为东经、y 为北纬，单位为度。 */
    struct Entity {
        QString name;
        QString primaryPrefix;
        QPointF coordinates;
    };

    /** @return 进程内共享的不可变数据库，首次调用时加载。 */
    static const DxccDatabase &instance();
    /**
     * @brief 查询呼号或前缀，忽略大小写和首尾空白；完整呼号例外优先于最长前缀。
     * @return DXCC 代表坐标；未知、歧义或 /MM、/AM 等无固定 DXCC 的输入返回空值。
     */
    std::optional<Entity> find(const QString &callsignOrPrefix) const;

private:
    DxccDatabase();
    std::optional<Entity> findSimple(const QString &text) const;

    QHash<QString, Entity> m_prefixes;
    QHash<QString, Entity> m_callsigns;
    QHash<QString, Entity> m_entities;
};
