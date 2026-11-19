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

#include "CrDelimitedBuffer.h"

QList<QByteArray> CrDelimitedBuffer::append(const QByteArray &bytes)
{
    QList<QByteArray> completed;
    for (const char byte : bytes) {
        if (m_skipLfAfterCr) {
            m_skipLfAfterCr = false;
            if (byte == '\n')
                continue;
        }

        if (byte == '\r') {
            m_pending.append(byte);
            completed.append(m_pending);
            m_pending.clear();
            m_skipLfAfterCr = true;
        } else {
            m_pending.append(byte);
        }
    }
    return completed;
}

void CrDelimitedBuffer::clear()
{
    m_pending.clear();
    m_skipLfAfterCr = false;
}

qsizetype CrDelimitedBuffer::pendingSize() const
{
    return m_pending.size();
}
