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

#include "NetworkPortConfigDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

NetworkPortConfigDialog::NetworkPortConfigDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("networkPortConfigDialog"));
    setWindowTitle(tr("Network port configuration"));
    setWindowModality(Qt::WindowModal);
    resize(470, 330);

    auto *layout = new QGridLayout(this);
    m_mode = new QComboBox(this);
    m_mode->setObjectName(QStringLiteral("ethernetMode"));
    m_mode->addItem(tr("DHCP"), QStringLiteral("dhcp"));
    m_mode->addItem(tr("Static IPv4"), QStringLiteral("static"));
    layout->addWidget(new QLabel(tr("Address mode"), this), 0, 0);
    layout->addWidget(m_mode, 0, 1);

    auto addField = [this, layout](int row, const QString &labelText,
                                   const QString &objectName, const QString &placeholder) {
        auto *field = new QLineEdit(this);
        field->setObjectName(objectName);
        field->setPlaceholderText(placeholder);
        layout->addWidget(new QLabel(labelText, this), row, 0);
        layout->addWidget(field, row, 1);
        return field;
    };
    m_address = addField(1, tr("IPv4 address"), QStringLiteral("ethernetAddress"),
                         QStringLiteral("192.168.1.50"));
    m_netmask = addField(2, tr("Netmask"), QStringLiteral("ethernetNetmask"),
                         QStringLiteral("255.255.255.0"));
    m_gateway = addField(3, tr("Gateway"), QStringLiteral("ethernetGateway"),
                         QStringLiteral("192.168.1.1"));
    m_dns1 = addField(4, tr("DNS 1"), QStringLiteral("ethernetDns1"),
                      QStringLiteral("192.168.1.1"));
    m_dns2 = addField(5, tr("DNS 2"), QStringLiteral("ethernetDns2"),
                      QStringLiteral("8.8.8.8"));
    layout->setColumnStretch(1, 1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_apply = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    m_apply->setObjectName(QStringLiteral("applyNetworkPort"));
    layout->addWidget(buttons, 6, 0, 1, 2);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        EthernetConfiguration configuration;
        configuration.mode = m_mode->currentData().toString();
        configuration.address = m_address->text().trimmed();
        configuration.netmask = m_netmask->text().trimmed();
        configuration.gateway = m_gateway->text().trimmed();
        configuration.dns1 = m_dns1->text().trimmed();
        configuration.dns2 = m_dns2->text().trimmed();
        emit configurationRequested(configuration);
    });
    connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &NetworkPortConfigDialog::updateControls);
    connect(m_address, &QLineEdit::textChanged, this, &NetworkPortConfigDialog::updateControls);
    connect(m_netmask, &QLineEdit::textChanged, this, &NetworkPortConfigDialog::updateControls);
    updateControls();
}

void NetworkPortConfigDialog::setConnectionState(bool commandEnabled)
{
    m_commandEnabled = commandEnabled;
    updateControls();
}

void NetworkPortConfigDialog::updateControls()
{
    const bool staticMode = m_mode->currentData().toString() == QStringLiteral("static");
    m_address->setEnabled(staticMode);
    m_netmask->setEnabled(staticMode);
    m_gateway->setEnabled(staticMode);
    m_dns1->setEnabled(staticMode);
    m_dns2->setEnabled(staticMode);
    m_apply->setEnabled(m_commandEnabled &&
                        (!staticMode || (!m_address->text().trimmed().isEmpty() &&
                                         !m_netmask->text().trimmed().isEmpty())));
}
