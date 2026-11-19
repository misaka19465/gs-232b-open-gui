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

#include "MainWindow.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "NetworkAddresses.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QStringList>

void MainWindow::getIpAddresses()
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    // NOTE: NET GET 与 PROV STATUS 均为只读查询，网络连接也无需管理认证。
    sendManagementCommand(
        QStringLiteral("!NET GET"), QStringLiteral("NET GET"),
        [this](const QJsonObject &network) {
            sendManagementCommand(
                QStringLiteral("!PROV STATUS"), QStringLiteral("PROV STATUS"),
                [this, network](const QJsonObject &provisioning) {
                    const auto addresses = NetworkAddresses::fromResponses(network, provisioning);
                    QStringList wireless;
                    if (!addresses.station.isEmpty())
                        wireless.append(addresses.station);
                    if (!addresses.accessPoint.isEmpty())
                        wireless.append(tr("%1 (AP provisioning)").arg(addresses.accessPoint));

                    auto *dialog = new QMessageBox(
                        QMessageBox::Information, tr("Get IP"),
                        tr("Wired IP: %1\nWireless IP: %2")
                            .arg(addresses.ethernet.isEmpty() ? tr("Not connected")
                                                             : addresses.ethernet,
                                 wireless.isEmpty() ? tr("Not connected")
                                                    : wireless.join(QLatin1Char('/'))),
                        QMessageBox::Ok, this);
                    dialog->setObjectName(QStringLiteral("networkAddressesDialog"));
                    dialog->button(QMessageBox::Ok)->setText(tr("OK"));
                    dialog->setTextFormat(Qt::PlainText);
                    dialog->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                                    Qt::TextSelectableByKeyboard);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->setWindowModality(Qt::WindowModal);
                    dialog->open();
                });
        });
}
