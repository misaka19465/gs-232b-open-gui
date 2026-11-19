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

#include "ControlPanel.h"
#include "AnglePresets.h"
#include "AppSettings.h"
#include "Compass.h"
#include "CompassLayout.h"
#include "MotionButtons.h"
#include "targeting/TargetBearing.h"

#include <QCheckBox>
#include <QFrame>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {

void limitInputWidth(QLineEdit *input)
{
    const int textWidth = QFontMetrics(input->font()).horizontalAdvance(QString(10, QLatin1Char('W')));
    const int frameWidth = input->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, input);
    input->setMaximumWidth(textWidth + frameWidth * 2 + 12);
    input->setAlignment(Qt::AlignCenter);
}

} // namespace

ControlPanel::ControlPanel(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("controlPanel"));
    m_compass = new Compass(this);
    m_compass->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *controls = new QWidget(this);
    new CompassLayout(m_compass, controls, this);
    auto *layout = new QHBoxLayout(controls);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    connect(m_compass, &Compass::mapError, this, &ControlPanel::mapError);
    connect(m_compass, &Compass::angleRequested, this, [this](int angle) {
        requestAzimuth(m_azimuthScale.mechanicalBearing(angle, m_azimuthValue));
    });
    auto *azimuthControls = new QWidget(controls);
    azimuthControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *azimuthColumn = new QVBoxLayout(azimuthControls);
    azimuthColumn->setContentsMargins(0, 0, 0, 0);
    auto *mode = new QGroupBox(tr("Mode"), azimuthControls);
    mode->setObjectName(QStringLiteral("controlMode"));
    auto *modeLayout = new QVBoxLayout(mode);
    auto *manual = new QRadioButton(tr("Manual"), mode);
    manual->setObjectName(QStringLiteral("manualMode"));
    manual->setToolTip(tr("Ignore control commands from bridge apps and continue reporting measured positions."));
    manual->setChecked(true);
    auto *tracking = new QRadioButton(tr("Tracking"), mode);
    tracking->setObjectName(QStringLiteral("trackingMode"));
    tracking->setToolTip(tr("Allow bridge apps to control the rotator."));
    const AppSettings preferences;
    m_windProtectionEnabled = preferences.windProtectionEnabled();
    m_windWeatherProvider = preferences.windWeatherProvider();
    auto *windMode = new QRadioButton(tr("Wind protection"), mode);
    windMode->setObjectName(QStringLiteral("windProtectionMode"));
    windMode->setChecked(m_windProtectionEnabled);
    windMode->setToolTip(
        tr("Automatically point the antenna using the configured wind direction offset."));
    auto *longPath = new QCheckBox(tr("LP"), mode);
    longPath->setObjectName(QStringLiteral("longPath"));
    longPath->setToolTip(tr("Use long path for grid, callsign or DXCC targets and GridTracker. Direct angles and N1MM targets are unchanged."));
    modeLayout->addWidget(manual);
    modeLayout->addWidget(tracking);
    modeLayout->addWidget(windMode);
    modeLayout->addWidget(longPath);
    connect(tracking, &QRadioButton::toggled, this, [this](bool enabled) {
        cancelPendingMotion();
        m_trackingEnabled = enabled;
        emit trackingEnabledChanged(enabled);
    });
    connect(longPath, &QCheckBox::toggled, this, [this](bool enabled) {
        cancelPendingMotion();
        m_longPathEnabled = enabled;
        emit longPathEnabledChanged(enabled);
    });

    m_windStatus = new QLabel(mode);
    m_windStatus->setObjectName(QStringLiteral("windDirectionStatus"));
    m_windStatus->setTextFormat(Qt::RichText);
    m_windStatus->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_windStatus->setOpenExternalLinks(true);
    m_windStatus->setToolTip(tr("Weather data attribution"));
    updateWindStatus();
    modeLayout->addWidget(m_windStatus);
    connect(windMode, &QRadioButton::toggled, this, [this](bool enabled) {
        m_windProtectionEnabled = enabled;
        emit windProtectionEnabledChanged(enabled);
    });
    azimuthColumn->addWidget(mode);
    azimuthColumn->addStretch(1);
    m_azimuth = new QLabel(azimuthControls);
    m_azimuth->setObjectName(QStringLiteral("azimuthReading"));
    m_azimuth->setAlignment(Qt::AlignCenter);
    m_azimuth->setAccessibleName(tr("Azimuth angle"));
    azimuthColumn->addWidget(m_azimuth);
    m_azimuthInput = new QLineEdit(azimuthControls);
    m_azimuthInput->setObjectName(QStringLiteral("azimuthInput"));
    m_azimuthInput->setMaxLength(64);
    limitInputWidth(m_azimuthInput);
    m_azimuthInput->setPlaceholderText(tr("Target"));
    m_azimuthInput->setAccessibleName(tr("Azimuth angle, grid or callsign"));
    m_azimuthInput->setToolTip(tr("Enter an angle, grid, callsign or prefix. Press Enter to move."));
    connect(m_azimuthInput, &QLineEdit::returnPressed, this, &ControlPanel::submitAzimuthInput);
    azimuthColumn->addWidget(m_azimuthInput, 0, Qt::AlignHCenter);
    azimuthColumn->addStretch(1);
    m_azimuthMotion = new MotionButtons(Qt::Horizontal, azimuthControls);
    m_azimuthMotion->setMaximumHeight(42);
    azimuthColumn->addWidget(m_azimuthMotion);
    layout->addWidget(azimuthControls, 1);
    auto *azimuthPresetGroup = new QGroupBox(tr("Presets"), controls);
    auto *azimuthPresetLayout = new QVBoxLayout(azimuthPresetGroup);
    m_azimuthPresets = new AnglePresets(AppSettings::Axis::Azimuth, azimuthPresetGroup);
    m_azimuthPresets->setTargetResolver([this](const QString &text) {
        QString error;
        return AnglePresets::TargetInputResult{resolveTargetBearing(text, error), error};
    });
    connect(m_azimuthPresets, &AnglePresets::inputRejected, this, &ControlPanel::inputError);
    azimuthPresetLayout->addWidget(m_azimuthPresets);
    layout->addWidget(azimuthPresetGroup, 1);

    connect(m_azimuthPresets, &AnglePresets::angleRequested, this, [this](int mechanical) {
        requestAzimuth(m_shortestPathEnabled
                           ? m_azimuthScale.mechanicalBearing(m_azimuthScale.corrected(mechanical), m_azimuthValue)
                           : mechanical);
    });

    m_elevationControls = new QWidget(controls);
    m_elevationControls->setObjectName(QStringLiteral("elevationControls"));
    m_elevationControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *elevationLayout = new QHBoxLayout(m_elevationControls);
    elevationLayout->setContentsMargins(0, 0, 0, 0);
    elevationLayout->setSpacing(10);
    auto *separator = new QFrame(m_elevationControls);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    elevationLayout->addWidget(separator);

    auto *elevationColumn = new QVBoxLayout;
    auto *elevationLabel = new QLabel(tr("Elevation"), m_elevationControls);
    elevationLabel->setAlignment(Qt::AlignCenter);
    elevationColumn->addWidget(elevationLabel);
    m_slider = new QSlider(Qt::Vertical, m_elevationControls);
    m_slider->setObjectName(QStringLiteral("elevationSlider"));
    m_slider->setRange(0, 180);
    m_slider->setMinimumHeight(90);
    m_slider->setAccessibleName(tr("Elevation angle"));
    m_elevationCommandTimer = new QTimer(this);
    m_elevationCommandTimer->setSingleShot(true);
    m_elevationCommandTimer->setInterval(150);
    m_elevation = new QLabel(m_elevationControls);
    m_elevation->setObjectName(QStringLiteral("elevationReading"));
    m_elevation->setAlignment(Qt::AlignCenter);
    m_elevation->setAccessibleName(tr("Elevation angle"));
    elevationColumn->addWidget(m_slider, 1, Qt::AlignHCenter);
    elevationColumn->addWidget(m_elevation);
    m_elevationInput = new QLineEdit(m_elevationControls);
    m_elevationInput->setObjectName(QStringLiteral("elevationInput"));
    m_elevationInput->setMaxLength(64);
    limitInputWidth(m_elevationInput);
    m_elevationInput->setPlaceholderText(tr("Angle"));
    m_elevationInput->setAccessibleName(tr("Elevation angle"));
    m_elevationInput->setToolTip(tr("Enter an angle from 0 to 180° and press Enter to move."));
    connect(m_elevationInput, &QLineEdit::returnPressed, this, &ControlPanel::submitElevationInput);
    elevationColumn->addWidget(m_elevationInput, 0, Qt::AlignHCenter);
    elevationLayout->addLayout(elevationColumn, 1);
    m_elevationMotion = new MotionButtons(Qt::Vertical, m_elevationControls);
    m_elevationMotion->setMaximumHeight(150);
    elevationLayout->addWidget(m_elevationMotion, 0, Qt::AlignVCenter);
    auto *elevationPresetGroup = new QGroupBox(tr("Presets"), m_elevationControls);
    auto *elevationPresetLayout = new QVBoxLayout(elevationPresetGroup);
    m_elevationPresets = new AnglePresets(AppSettings::Axis::Elevation, elevationPresetGroup);
    elevationPresetLayout->addWidget(m_elevationPresets);
    elevationLayout->addWidget(elevationPresetGroup, 1);
    layout->addWidget(m_elevationControls, 2);

    connect(m_elevationPresets, &AnglePresets::angleRequested, this, &ControlPanel::requestElevation);
    connect(m_elevationPresets, &AnglePresets::inputRejected, this, &ControlPanel::inputError);

    connect(m_slider, &QSlider::valueChanged, this, [this] {
        if (m_commandEnabled && m_elevationSupported)
            m_elevationCommandTimer->start();
    });
    connect(m_elevationCommandTimer, &QTimer::timeout, this, [this] {
        requestElevation(m_slider->value());
    });

    setupMotionButtons();
    m_shortestPathEnabled = preferences.shortestPathEnabled();
    m_azimuthPresets->setShortestPathEnabled(m_shortestPathEnabled);
    setAzimuthOffset(preferences.azimuthOffset());
    setCommandEnabled(false);
}

void ControlPanel::setElevationSupported(bool supported)
{
    if (m_elevationSupported == supported)
        return;
    m_elevationSupported = supported;
    m_elevationCommandTimer->stop();
    if (!supported) {
        m_elevationMotion->cancelMotion();
        m_elevationInput->clear();
    }
    m_elevationControls->setVisible(supported);
    setCommandEnabled(m_commandEnabled);
}

bool ControlPanel::elevationSupported() const
{
    return m_elevationSupported;
}

void ControlPanel::setShortestPathEnabled(bool enabled)
{
    m_shortestPathEnabled = enabled;
    cancelPendingMotion();
    updateAzimuthInput();
    m_azimuthPresets->setShortestPathEnabled(enabled);
    setPosition(m_azimuthValue, m_elevationValue);
}

void ControlPanel::setAzimuthOffset(int degrees)
{
    m_azimuthScale = AzimuthScale(degrees);
    cancelPendingMotion();
    updateAzimuthInput();
    m_azimuthPresets->setAzimuthOffset(degrees);
    setPosition(m_azimuthValue, m_elevationValue);
}

void ControlPanel::updateAzimuthInput()
{
    m_azimuthInput->clear();
    m_azimuthInput->setToolTip(m_shortestPathEnabled
        ? tr("Enter an integer angle, grid, callsign or prefix. Angles wrap at 360° and use the shortest reachable rotation. Press Enter to move.")
        : tr("Enter an angle (%1–%2°), grid, callsign or prefix. Press Enter to move.")
              .arg(m_azimuthScale.minimum()).arg(m_azimuthScale.maximum()));
}

void ControlPanel::submitAzimuthInput()
{
    const QString text = m_azimuthInput->text().trimmed();
    const qint64 minimum = m_shortestPathEnabled ? 0 : m_azimuthScale.minimum();
    const qint64 maximum = m_shortestPathEnabled ? 359 : m_azimuthScale.maximum();
    bool numeric = false;
    qint64 angle = text.toLongLong(&numeric);
    if (numeric && m_shortestPathEnabled)
        angle = AzimuthScale::normalized(angle);

    if (numeric) {
        if (angle < minimum || angle > maximum) {
            emit inputError(m_shortestPathEnabled
                                ? tr("Enter an integer angle.")
                                : tr("Enter an angle from %1 to %2°.").arg(minimum).arg(maximum));
            return;
        }
        if (m_shortestPathEnabled)
            requestAzimuth(m_azimuthScale.mechanicalBearing(angle, m_azimuthValue));
        else if (const auto mechanical = m_azimuthScale.mechanical(angle))
            requestAzimuth(*mechanical);
        return;
    }

    static const QRegularExpression integerPattern(QStringLiteral("^[+-]?[0-9]+$"));
    if (integerPattern.match(text).hasMatch()) {
        emit inputError(m_shortestPathEnabled
                            ? tr("Enter an integer angle.")
                            : tr("Enter an angle from %1 to %2°.").arg(minimum).arg(maximum));
        return;
    }

    QString error;
    const auto bearing = resolveTargetBearing(text, error);
    if (!bearing) {
        emit inputError(error);
        return;
    }
    requestAzimuth(m_azimuthScale.mechanicalBearing(*bearing, m_azimuthValue));
}

std::optional<int> ControlPanel::resolveTargetBearing(const QString &text, QString &error) const
{
    const auto result = TargetBearing::calculate(m_stationGrid, text);
    if (!result.solution) {
        switch (result.error) {
        case TargetBearing::Error::InvalidStationGrid:
            error = tr("Set your grid in Settings → My grid… before using a grid or callsign.");
            break;
        case TargetBearing::Error::UndefinedBearing:
            error = tr("This target has no unique bearing from your grid.");
            break;
        case TargetBearing::Error::UnknownTarget:
        case TargetBearing::Error::None:
            error = tr("Unknown target. Enter a 4, 6 or 8 character grid, callsign or DXCC prefix.");
            break;
        }
        return std::nullopt;
    }

    const int shortBearing = result.solution->roundedDegrees();
    return m_longPathEnabled ? AzimuthScale::normalized(shortBearing + 180) : shortBearing;
}

void ControlPanel::submitElevationInput()
{
    bool ok = false;
    const int angle = m_elevationInput->text().trimmed().toInt(&ok);
    if (!ok || angle < 0 || angle > 180) {
        emit inputError(tr("Enter an angle from 0 to 180°."));
        return;
    }
    requestElevation(angle);
}

void ControlPanel::setCommandEnabled(bool enabled)
{
    m_commandEnabled = enabled;
    if (!enabled)
        cancelPendingMotion();
    m_compass->setEnabled(enabled);
    m_slider->setEnabled(enabled && m_elevationSupported);
    m_azimuthMotion->setEnabled(enabled);
    m_elevationMotion->setEnabled(enabled && m_elevationSupported);
    if (!enabled) {
        m_azimuthInput->clear();
        m_elevationInput->clear();
    }
    m_azimuthInput->setEnabled(enabled);
    m_elevationInput->setEnabled(enabled && m_elevationSupported);
    // NOTE: 预设编辑属于本地偏好设置，断开或透传时仍可编辑；发送端会校验真实连接状态。
    m_azimuthPresets->setEnabled(true);
    m_elevationPresets->setEnabled(true);
}

void ControlPanel::setPosition(int azimuth, int elevation)
{
    m_azimuthValue = qBound(0, azimuth, 450);
    m_elevationValue = qBound(0, elevation, 180);
    m_azimuth->setText(QStringLiteral("%1°").arg(m_azimuthScale.corrected(m_azimuthValue),
                                                 3, 10, QLatin1Char('0')));
    m_elevation->setText(QStringLiteral("%1°").arg(m_elevationValue, 3, 10, QLatin1Char('0')));
    // NOTE: 取模仅影响方位显示与目标选解，内部反馈和 W 命令始终保留机械行程。
    const qint64 corrected = m_azimuthScale.corrected(m_azimuthValue);
    m_compass->setAngle(AzimuthScale::normalized(corrected));
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(m_elevationValue);
}

int ControlPanel::azimuth() const
{
    return m_azimuthValue;
}

void ControlPanel::setGreatCircleMapEnabled(bool enabled)
{
    m_compass->setGreatCircleMapEnabled(enabled);
}

void ControlPanel::setStationGrid(const QString &grid)
{
    m_stationGrid = grid.trimmed().toUpper();
    m_compass->setStationGrid(grid);
}

void ControlPanel::setAntennaGain(double gain)
{
    m_compass->setAntennaGain(gain);
}

void ControlPanel::setWindDirection(int direction)
{
    m_windDirection = direction >= 0 ? AzimuthScale::normalized(direction) : -1;
    updateWindStatus();
}

void ControlPanel::setWindWeatherProvider(WindWeather::Provider provider)
{
    if (m_windWeatherProvider == provider)
        return;
    m_windWeatherProvider = provider;
    m_windDirection = -1;
    updateWindStatus();
}

void ControlPanel::updateWindStatus()
{
    const QString direction = m_windDirection >= 0
        ? QString::number(m_windDirection)
        : QStringLiteral("--");
    if (m_windWeatherProvider == WindWeather::Provider::OpenMeteo) {
        m_windStatus->setText(
            tr("Wind from %1° · <a href=\"https://open-meteo.com/\">Open-Meteo</a> · <a href=\"https://creativecommons.org/licenses/by/4.0/\">CC BY 4.0</a>")
                .arg(direction));
        return;
    }
    if (m_windWeatherProvider == WindWeather::Provider::MetNorway) {
        m_windStatus->setText(
            tr("Wind from %1° · <a href=\"https://www.met.no/en\">MET Norway</a> · <a href=\"https://creativecommons.org/licenses/by/4.0/\">CC BY 4.0</a>")
                .arg(direction));
        return;
    }
    m_windStatus->setText(
        tr("Wind from %1° · <a href=\"https://www.qweather.com\">QWeather</a> · <a href=\"https://developer.qweather.com/attribution.html\">Attribution</a>")
            .arg(direction));
}

void ControlPanel::requestAzimuthBearing(int bearing)
{
    requestAzimuth(m_azimuthScale.mechanicalBearing(bearing, m_azimuthValue));
}

int ControlPanel::elevation() const
{
    return m_elevationValue;
}
