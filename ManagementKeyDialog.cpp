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

#include "ManagementKeyDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ManagementKeyDialog::ManagementKeyDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("managementKeyDialog"));
    setWindowTitle(tr("Management password"));
    setWindowModality(Qt::WindowModal);
    resize(380, 190);
    auto *layout = new QVBoxLayout(this);
    m_password = new QLineEdit(this);
    m_password->setObjectName(QStringLiteral("managementPassword"));
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Enter password"));
    auto *label = new QLabel(tr("Management password"), this);
    label->setBuddy(m_password);
    layout->addWidget(label);
    layout->addWidget(m_password);
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_continue = buttons->addButton(tr("Authenticate"), QDialogButtonBox::AcceptRole);
    m_continue->setObjectName(QStringLiteral("authorizeButton"));
    m_continue->setEnabled(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_password, &QLineEdit::textChanged, this, &ManagementKeyDialog::updateControls);
}

QString ManagementKeyDialog::password() const
{
    return m_password->text();
}

void ManagementKeyDialog::updateControls()
{
    m_continue->setEnabled(!m_password->text().isEmpty());
}
