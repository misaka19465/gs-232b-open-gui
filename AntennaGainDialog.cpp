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

#include "AntennaGainDialog.h"
#include "AppSettings.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

AntennaGainDialog::AntennaGainDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("antennaGainDialog"));
    setWindowTitle(tr("Antenna gain"));
    setWindowModality(Qt::WindowModal);
    resize(440, 240);

    auto *layout = new QVBoxLayout(this);
    m_gain = new QDoubleSpinBox(this);
    m_gain->setObjectName(QStringLiteral("antennaGain"));
    m_gain->setRange(AppSettings::minimumAntennaGain(), AppSettings::maximumAntennaGain());
    m_gain->setDecimals(1);
    m_gain->setSingleStep(0.5);
    m_gain->setValue(AppSettings().antennaGain());
    auto *label = new QLabel(tr("Antenna gain (dBi)"), this);
    label->setBuddy(m_gain);
    layout->addWidget(label);
    layout->addWidget(m_gain);

    auto *hint = new QLabel(
        tr("Reference only: higher gain produces a narrower main lobe. The lobe is shown only on the great-circle map; 0 dBi hides it."),
        this);
    hint->setWordWrap(true);
    m_gain->setAccessibleDescription(hint->text());
    layout->addWidget(hint);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    auto *save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    save->setObjectName(QStringLiteral("saveAntennaGain"));
    save->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &AntennaGainDialog::saveGain);
}

void AntennaGainDialog::saveGain()
{
    m_gain->interpretText();
    if (!AppSettings().setAntennaGain(m_gain->value())) {
        QMessageBox::warning(this, tr("Antenna gain"), tr("Could not save the antenna gain."));
        return;
    }
    accept();
}
