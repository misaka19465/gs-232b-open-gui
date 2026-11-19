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

#include "FaultStatusPanel.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

FaultStatusPanel::FaultStatusPanel(FaultStatusMonitor *monitor, QWidget *parent)
    : QToolButton(parent), m_monitor(monitor)
{
    setObjectName(QStringLiteral("faultStatusPanel"));
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Fault status"));

    m_details = new QDialog(this);
    m_details->setObjectName(QStringLiteral("faultStatusDialog"));
    m_details->setWindowTitle(tr("Fault status"));
    m_details->setMinimumWidth(360);
    auto *layout = new QVBoxLayout(m_details);
    m_status = new QLabel(m_details);
    m_status->setObjectName(QStringLiteral("faultStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_result = new QLabel(m_details);
    m_result->setObjectName(QStringLiteral("faultClearResult"));
    m_result->setTextFormat(Qt::PlainText);
    m_result->setWordWrap(true);
    layout->addWidget(m_result);
    auto *buttons = new QDialogButtonBox(m_details);
    m_clear = buttons->addButton(tr("Clear fault"), QDialogButtonBox::ActionRole);
    m_clear->setObjectName(QStringLiteral("clearFault"));
    buttons->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, m_details, &QDialog::reject);
    connect(this, &QToolButton::clicked, this, [this] {
        // NOTE: 详情保持非模态，查看或清除故障时仍可使用主窗口的停止控件。
        m_details->show();
        m_details->raise();
        m_details->activateWindow();
    });
    connect(m_clear, &QPushButton::clicked, this, [this] {
        m_result->clear();
        m_result->hide();
        m_monitor->clearFault();
    });
    connect(m_monitor, &FaultStatusMonitor::stateChanged, this, &FaultStatusPanel::updateStatus);
    connect(m_monitor, &FaultStatusMonitor::clearFinished, this, &FaultStatusPanel::showClearResult);
    updateStatus();
}

void FaultStatusPanel::updateStatus()
{
    if (!m_monitor->isEnabled()) {
        m_status->setText(tr("Connect directly to read status"));
        m_result->clear();
        m_result->hide();
    } else if (m_monitor->isClearing()) {
        m_status->setText(tr("Clearing fault…"));
    } else if (m_monitor->fault() == FaultStatusMonitor::Fault::Unknown) {
        m_status->setText(m_monitor->isReading() ? tr("Reading status…")
                                                : tr("Unable to read device status"));
    } else {
        const QString description = faultDescription();
        const int axes = m_monitor->axisMask();
        if (m_monitor->fault() == FaultStatusMonitor::Fault::None || axes == 0)
            m_status->setText(tr("%1").arg(description));
        else if (axes == 1)
            m_status->setText(tr("Azimuth: %1").arg(description));
        else if (axes == 2)
            m_status->setText(tr("Elevation: %1").arg(description));
        else
            m_status->setText(tr("Both axes: %1").arg(description));
    }
    m_clear->setEnabled(m_monitor->canClear());
    updateIndicator();
}

void FaultStatusPanel::updateIndicator()
{
    QStyle::StandardPixmap icon = QStyle::SP_MessageBoxInformation;
    QString summary;
    if (!m_monitor->isEnabled()) {
        summary = tr("Status unavailable");
    } else if (m_monitor->isClearing()) {
        summary = tr("Clearing fault…");
    } else if (m_monitor->fault() == FaultStatusMonitor::Fault::Unknown) {
        summary = m_monitor->isReading() ? tr("Reading status…") : tr("Status unavailable");
        if (!m_monitor->isReading())
            icon = QStyle::SP_MessageBoxWarning;
    } else if (m_monitor->fault() == FaultStatusMonitor::Fault::None) {
        summary = tr("No faults");
        icon = QStyle::SP_DialogApplyButton;
    } else {
        summary = tr("Device fault");
        icon = QStyle::SP_MessageBoxCritical;
    }
    setText(summary);
    setIcon(style()->standardIcon(icon));
    QString description = m_status->text();
    if (!m_result->text().isEmpty())
        description += QLatin1Char('\n') + m_result->text();
    setAccessibleDescription(description);
    setToolTip(description + QLatin1Char('\n') + tr("Click to view fault details"));
}

QString FaultStatusPanel::faultDescription() const
{
    using Fault = FaultStatusMonitor::Fault;
    switch (m_monitor->fault()) {
    case Fault::None: return tr("No faults");
    case Fault::Sample: return tr("Position feedback unavailable");
    case Fault::Output: return tr("Motor output fault");
    case Fault::Limit: return tr("Travel limit fault");
    case Fault::FeedbackDirection: return tr("Unexpected rotation direction");
    case Fault::NoProgress: return tr("Rotator is not moving");
    case Fault::RunTimeout: return tr("Motion timed out");
    case Fault::SequenceLate: return tr("Sequence timing fault");
    case Fault::SequenceTarget: return tr("Sequence target unavailable");
    default: return tr("Device fault");
    }
}

void FaultStatusPanel::showClearResult(FaultStatusMonitor::ClearResult result)
{
    using Result = FaultStatusMonitor::ClearResult;
    switch (result) {
    case Result::Succeeded:
        m_result->clear();
        m_result->hide();
        updateIndicator();
        return;
    case Result::Busy:
        m_result->setText(tr("Stop motion and finish pending operations before clearing"));
        break;
    case Result::NotOwner:
        m_result->setText(tr("Another controller is using the device"));
        break;
    case Result::NotAuthorized:
        m_result->setText(tr("Clearing is not permitted on this connection"));
        break;
    case Result::FaultPersists:
        m_result->setText(tr("Fault persists; check feedback and hardware"));
        break;
    case Result::TimedOut:
        m_result->setText(tr("Clear timed out; checking device status"));
        break;
    case Result::Failed:
        m_result->setText(tr("Could not clear the fault"));
        break;
    }
    m_result->show();
    updateIndicator();
}
