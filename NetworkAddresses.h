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

#include <QJsonObject>
#include <QString>

/** @brief 设备正在使用的 IPv4 地址；空字符串表示对应接口未连接或地址无效。 */
struct NetworkAddresses
{
    QString ethernet;
    QString station;
    QString accessPoint;

    /** @brief 从 NET GET 和 PROV STATUS 的成功响应 data 读取实际地址，不使用已保存配置。 */
    static NetworkAddresses fromResponses(const QJsonObject &network,
                                         const QJsonObject &provisioning);
};
