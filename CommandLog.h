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

namespace Communication {

/**
 * @brief 为单条已编码命令生成日志副本，遮蔽管理命令 JSON 中所有嵌套的 password 字段。
 * @param bytes 以 CR 结尾的单条命令；非管理命令按原始字节保留。
 * @return 可显示的命令；无法解析的管理命令参数整体替换为 ***，保留命令名。
 */
QByteArray commandForLog(const QByteArray &bytes);

} // namespace Communication
