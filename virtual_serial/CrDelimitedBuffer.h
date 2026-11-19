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

/** @brief 按 CR 拆分任意大小字节块，容忍 CRLF，并保留未完成行。 */
class CrDelimitedBuffer final
{
public:
    /** @return 本次输入中所有完整的、包含 CR 的行。 */
    QList<QByteArray> append(const QByteArray &bytes);
    /** @brief 丢弃未完成行和 CRLF 可选后缀状态。 */
    void clear();
    /** @return 当前未完成行的字节数。 */
    qsizetype pendingSize() const;

private:
    QByteArray m_pending;
    bool m_skipLfAfterCr = false;
};
