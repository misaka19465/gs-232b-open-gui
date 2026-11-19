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

#include "AzimuthOffsetDialog.h"
#include "AppSettings.h"
#include "AzimuthScale.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

AzimuthOffsetDialog::AzimuthOffsetDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("azimuthOffsetDialog"));
    setWindowTitle(tr("Azimuth offset"));
    setWindowModality(Qt::WindowModal);
    resize(440, 240);

    auto *layout = new QVBoxLayout(this);
    m_offset = new QSpinBox(this);
    m_offset->setObjectName(QStringLiteral("azimuthOffset"));
    m_offset->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    m_offset->setValue(AppSettings().azimuthOffset());
    auto *label = new QLabel(tr("Azimuth offset (°)"), this);
    label->setBuddy(m_offset);
    layout->addWidget(label);
    layout->addWidget(m_offset);
    auto *hint = new QLabel(tr("Corrected azimuth = controller angle + offset. For example, −30° changes 0–450° to −30–420° (330–60° on the compass)."), this);
    hint->setWordWrap(true);
    m_offset->setAccessibleDescription(hint->text());
    layout->addWidget(hint);
    m_range = new QLabel(this);
    m_range->setWordWrap(true);
    layout->addWidget(m_range);
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    auto *save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    save->setObjectName(QStringLiteral("saveAzimuthOffset"));
    save->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &AzimuthOffsetDialog::saveOffset);
    connect(m_offset, qOverload<int>(&QSpinBox::valueChanged), this, &AzimuthOffsetDialog::updateRange);
    updateRange();
}

void AzimuthOffsetDialog::updateRange()
{
    const AzimuthScale scale(m_offset->value());
    m_range->setText(tr("Azimuth range: %1° to %2°").arg(scale.minimum()).arg(scale.maximum()));
}

void AzimuthOffsetDialog::saveOffset()
{
    m_offset->interpretText();
    if (!AppSettings().setAzimuthOffset(m_offset->value())) {
        QMessageBox::warning(this, tr("Azimuth offset"), tr("Could not save the azimuth offset."));
        return;
    }
    accept();
}
