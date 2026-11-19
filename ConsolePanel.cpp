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

#include "ConsolePanel.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

ConsolePanel::ConsolePanel(QWidget *parent) : QGroupBox(tr("CONSOLE"), parent)
{
    setObjectName(QStringLiteral("consolePanel"));
    auto *layout = new QVBoxLayout(this);
    m_output = new QPlainTextEdit(this);
    m_output->setObjectName(QStringLiteral("consoleOutput"));
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_output, 1);

    auto *inputRow = new QHBoxLayout;
    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("commandInput"));
    m_input->setPlaceholderText(tr("Enter command"));
    m_send = new QPushButton(tr("Send"), this);
    m_send->setObjectName(QStringLiteral("sendButton"));
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_send);
    layout->addLayout(inputRow);
    setCommandEnabled(false);
    connect(m_send, &QPushButton::clicked, this, &ConsolePanel::requestCommand);
    connect(m_input, &QLineEdit::returnPressed, this, &ConsolePanel::requestCommand);

    // NOTE: 控制台只发出用户输入，收发回显由真实传输信号驱动。
}

void ConsolePanel::setCommandEnabled(bool enabled)
{
    m_input->setEnabled(enabled);
    m_send->setEnabled(enabled);
}

void ConsolePanel::appendReceived(const QByteArray &bytes)
{
    appendTraffic(bytes, false);
}

void ConsolePanel::appendSent(const QByteArray &bytes)
{
    appendTraffic(bytes, true);
}

void ConsolePanel::appendError(const QString &error)
{
    if (!m_lineStart)
        m_output->insertPlainText(QStringLiteral("\n"));
    m_output->insertPlainText(QStringLiteral("! ") + error + QStringLiteral("\n"));
    m_output->ensureCursorVisible();
    m_lineStart = true;
}

void ConsolePanel::appendProcessOutput(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return;

    QString text = QString::fromLocal8Bit(bytes);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));
    if (m_lineStart)
        m_output->insertPlainText(QStringLiteral("# "));
    m_output->insertPlainText(text);
    m_lineStart = text.endsWith(QChar('\n'));
    m_output->ensureCursorVisible();
}

void ConsolePanel::appendNotice(const QString &message)
{
    if (!m_lineStart)
        m_output->insertPlainText(QStringLiteral("\n"));
    m_output->insertPlainText(QStringLiteral("* ") + message + QStringLiteral("\n"));
    m_output->ensureCursorVisible();
    m_lineStart = true;
}

void ConsolePanel::requestCommand()
{
    const QString command = m_input->text();
    if (command.trimmed().isEmpty())
        return;
    m_input->clear();
    emit commandRequested(command);
}

void ConsolePanel::appendTraffic(const QByteArray &bytes, bool outgoing)
{
    if (bytes.isEmpty())
        return;

    QString text = QString::fromUtf8(bytes);
    if (text.contains(QChar::ReplacementCharacter))
        text = QString::fromLatin1(bytes.toHex(' '));
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));

    if (m_lineStart)
        m_output->insertPlainText(outgoing ? QStringLiteral("> ") : QStringLiteral("< "));
    m_output->insertPlainText(text);
    m_lineStart = text.endsWith(QChar('\n'));
    m_output->ensureCursorVisible();
}
