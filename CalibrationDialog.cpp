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

#include "CalibrationDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

CalibrationDialog::CalibrationDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("calibrationDialog"));
    setWindowTitle(tr("Calibration"));
    resize(520, 420);

    auto *layout = new QVBoxLayout(this);
    auto *positionGroup = new QGroupBox(tr("Current angles"), this);
    auto *positionLayout = new QFormLayout(positionGroup);
    m_azimuthReading = new QLabel(positionGroup);
    m_azimuthReading->setObjectName(QStringLiteral("calibrationAzimuthReading"));
    m_azimuthReading->setAccessibleName(tr("Azimuth"));
    m_elevationReading = new QLabel(positionGroup);
    m_elevationReading->setObjectName(QStringLiteral("calibrationElevationReading"));
    m_elevationReading->setAccessibleName(tr("Elevation"));
    for (auto *reading : {m_azimuthReading, m_elevationReading}) {
        reading->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        reading->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    }
    positionLayout->addRow(tr("Azimuth"), m_azimuthReading);
    positionLayout->addRow(tr("Elevation"), m_elevationReading);
    layout->addWidget(positionGroup);
    setPosition(0, 0);

    auto *zeroGroup = new QGroupBox(tr("Zero calibration"), this);
    auto *zeroLayout = new QVBoxLayout(zeroGroup);
    auto *zeroNote = new QLabel(
        tr("Move the azimuth to its counter-clockwise limit or the elevation to 0°, then click the corresponding button to calibrate controller zero."),
        zeroGroup);
    zeroNote->setWordWrap(true);
    // NOTE: 按换行后的说明保留最小高度，避免窄窗口或大字体下文字被压缩。
    zeroNote->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    zeroLayout->addWidget(zeroNote);

    auto *calibrationButtons = new QHBoxLayout;
    m_azimuthZero = new QPushButton(tr("Azimuth zero"), zeroGroup);
    m_azimuthZero->setObjectName(QStringLiteral("azimuthZeroCalibration"));
    m_azimuthZero->setAutoDefault(false);
    m_elevationZero = new QPushButton(tr("Elevation zero"), zeroGroup);
    m_elevationZero->setObjectName(QStringLiteral("elevationZeroCalibration"));
    m_elevationZero->setAutoDefault(false);
    calibrationButtons->addWidget(m_azimuthZero);
    calibrationButtons->addWidget(m_elevationZero);
    zeroLayout->addLayout(calibrationButtons);
    layout->addWidget(zeroGroup);

    auto *fullScaleGroup = new QGroupBox(tr("Full-scale calibration"), this);
    auto *fullScaleLayout = new QVBoxLayout(fullScaleGroup);
    auto *fullScaleNote = new QLabel(
        tr("For full-scale calibration, adjust the potentiometer until the azimuth reading matches 360° or 450° plus azimuth offset, or the elevation reading matches 180°."),
        fullScaleGroup);
    fullScaleNote->setWordWrap(true);
    fullScaleNote->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    fullScaleLayout->addWidget(fullScaleNote);
    layout->addWidget(fullScaleGroup);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    auto *close = buttons->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    close->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_azimuthZero, &QPushButton::clicked,
            this, &CalibrationDialog::azimuthZeroCalibrationRequested);
    connect(m_elevationZero, &QPushButton::clicked,
            this, &CalibrationDialog::elevationZeroCalibrationRequested);
    setCommandEnabled(false);
}

void CalibrationDialog::setCommandEnabled(bool enabled)
{
    m_azimuthZero->setEnabled(enabled);
    m_elevationZero->setEnabled(enabled);
}

void CalibrationDialog::setPosition(qint64 azimuth, int elevation)
{
    m_azimuthReading->setText(QStringLiteral("%1°").arg(azimuth, 3, 10, QLatin1Char('0')));
    m_elevationReading->setText(QStringLiteral("%1°").arg(elevation, 3, 10, QLatin1Char('0')));
}
