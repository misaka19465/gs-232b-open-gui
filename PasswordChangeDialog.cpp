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

#include "PasswordChangeDialog.h"

#include <QByteArray>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

bool validPassword(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    if (bytes.size() < 8 || bytes.size() > 64)
        return false;
    for (const unsigned char byte : bytes) {
        if (byte < 0x20 || byte > 0x7e)
            return false;
    }
    return true;
}

} // namespace

PasswordChangeDialog::PasswordChangeDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("passwordChangeDialog"));
    setWindowTitle(tr("Change management password"));
    setWindowModality(Qt::WindowModal);
    resize(420, 250);

    auto *layout = new QVBoxLayout(this);
    m_password = new QLineEdit(this);
    m_password->setObjectName(QStringLiteral("newManagementPassword"));
    m_password->setEchoMode(QLineEdit::Password);
    auto *passwordLabel = new QLabel(tr("New management password"), this);
    passwordLabel->setBuddy(m_password);
    layout->addWidget(passwordLabel);
    layout->addWidget(m_password);

    m_confirmation = new QLineEdit(this);
    m_confirmation->setObjectName(QStringLiteral("confirmManagementPassword"));
    m_confirmation->setEchoMode(QLineEdit::Password);
    auto *confirmationLabel = new QLabel(tr("Confirm password"), this);
    confirmationLabel->setBuddy(m_confirmation);
    layout->addWidget(confirmationLabel);
    layout->addWidget(m_confirmation);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_change = buttons->addButton(tr("Change password"), QDialogButtonBox::AcceptRole);
    m_change->setObjectName(QStringLiteral("changeManagementPassword"));
    m_change->setEnabled(false);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_password, &QLineEdit::textChanged, this, &PasswordChangeDialog::updateControls);
    connect(m_confirmation, &QLineEdit::textChanged, this, &PasswordChangeDialog::updateControls);
}

QString PasswordChangeDialog::password() const
{
    return m_password->text();
}

void PasswordChangeDialog::updateControls()
{
    m_change->setEnabled(validPassword(m_password->text()) &&
                         m_password->text() == m_confirmation->text());
}
