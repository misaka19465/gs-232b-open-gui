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

#include "WifiProvisioningDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

WifiProvisioningDialog::WifiProvisioningDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("wifiProvisioningDialog"));
    setWindowTitle(tr("Wi-Fi provisioning"));
    setWindowModality(Qt::WindowModal);
    resize(440, 250);
    auto *layout = new QVBoxLayout(this);
    m_ssid = new QLineEdit(this);
    m_ssid->setObjectName(QStringLiteral("wifiSsid"));
    m_ssid->setPlaceholderText(tr("SSID"));
    auto *ssidLabel = new QLabel(tr("Wi-Fi network"), this);
    ssidLabel->setBuddy(m_ssid);
    layout->addWidget(ssidLabel);
    layout->addWidget(m_ssid);
    m_password = new QLineEdit(this);
    m_password->setObjectName(QStringLiteral("wifiPassword"));
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Password"));
    auto *passwordLabel = new QLabel(tr("Wi-Fi password"), this);
    passwordLabel->setBuddy(m_password);
    layout->addWidget(passwordLabel);
    layout->addWidget(m_password);
    layout->addWidget(new QLabel(tr("2.4 GHz"), this));
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_configure = buttons->addButton(tr("Configure"), QDialogButtonBox::ActionRole);
    m_configure->setObjectName(QStringLiteral("configureWifi"));
    m_configure->setEnabled(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_configure, &QPushButton::clicked, this, [this] {
        emit configurationRequested(m_ssid->text().trimmed(), m_password->text());
    });
    connect(m_ssid, &QLineEdit::textChanged, this, &WifiProvisioningDialog::updateControls);
    connect(m_password, &QLineEdit::textChanged, this, &WifiProvisioningDialog::updateControls);
}

void WifiProvisioningDialog::setConnectionState(bool commandEnabled)
{
    m_commandEnabled = commandEnabled;
    updateControls();
}

void WifiProvisioningDialog::updateControls()
{
    m_configure->setEnabled(m_commandEnabled && !m_ssid->text().trimmed().isEmpty());
}
