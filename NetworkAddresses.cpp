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

#include "NetworkAddresses.h"

#include <QHostAddress>

namespace {

QString ipv4Address(const QJsonObject &interface)
{
    const QHostAddress address(interface.value(QStringLiteral("address")).toString());
    if (address.protocol() != QAbstractSocket::IPv4Protocol || address.toIPv4Address() == 0)
        return {};
    return address.toString();
}

} // namespace

NetworkAddresses NetworkAddresses::fromResponses(const QJsonObject &network,
                                                const QJsonObject &provisioning)
{
    NetworkAddresses addresses;
    const QJsonObject runtime = network.value(QStringLiteral("runtime")).toObject();
    const QJsonObject ethernet = runtime.value(QStringLiteral("eth")).toObject();
    if (ethernet.value(QStringLiteral("link_up")).toBool() &&
        ethernet.value(QStringLiteral("ipv4_valid")).toBool())
        addresses.ethernet = ipv4Address(ethernet);

    const QJsonObject wifi = runtime.value(QStringLiteral("wifi")).toObject();
    if (wifi.value(QStringLiteral("connected")).toBool() &&
        wifi.value(QStringLiteral("ipv4_valid")).toBool())
        addresses.station = ipv4Address(wifi);

    // NOTE: 固件在 AP 关闭后仍返回预设地址，只有活动配网窗口中的地址才可展示。
    if (provisioning.value(QStringLiteral("state")).toString() == QStringLiteral("active"))
        addresses.accessPoint = ipv4Address(provisioning.value(QStringLiteral("ap")).toObject());
    return addresses;
}
