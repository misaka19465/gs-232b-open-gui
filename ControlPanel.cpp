#include "ControlPanel.h"
#include "AngleReading.h"
#include "AnglePresets.h"
#include "AppSettings.h"
#include "Compass.h"
#include "CompassLayout.h"
#include "MotionButtons.h"
#include "targeting/TargetBearing.h"

#include <QCheckBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

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
    auto *longPath = new QCheckBox(tr("LP"), mode);
    longPath->setObjectName(QStringLiteral("longPath"));
    longPath->setToolTip(tr("Use long path for grid, callsign or DXCC targets and GridTracker. Direct angles and N1MM targets are unchanged."));
    modeLayout->addWidget(manual);
    modeLayout->addWidget(tracking);
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
    azimuthColumn->addWidget(mode);
    azimuthColumn->addStretch(1);
    auto *azimuthLabel = new QLabel(tr("Azimuth"), azimuthControls);
    azimuthLabel->setAlignment(Qt::AlignCenter);
    azimuthColumn->addWidget(azimuthLabel);
    m_azimuth = new AngleReading(450, azimuthControls);
    m_azimuth->setObjectName(QStringLiteral("azimuthReading"));
    connect(m_azimuth, &AngleReading::inputRejected, this, &ControlPanel::inputError);
    m_azimuth->setAccessibleName(tr("Azimuth angle, grid or callsign"));
    connect(m_azimuth, &AngleReading::angleRequested, this, [this](qint64 angle) {
        if (m_shortestPathEnabled)
            requestAzimuth(m_azimuthScale.mechanicalBearing(angle, m_azimuthValue));
        else if (const auto mechanical = m_azimuthScale.mechanical(angle))
            requestAzimuth(*mechanical);
    });
    azimuthColumn->addWidget(m_azimuth);
    azimuthColumn->addStretch(1);
    m_azimuthMotion = new MotionButtons(Qt::Horizontal, azimuthControls);
    m_azimuthMotion->setMaximumHeight(42);
    azimuthColumn->addWidget(m_azimuthMotion);
    layout->addWidget(azimuthControls, 1);
    auto *azimuthPresetGroup = new QGroupBox(tr("Presets"), controls);
    auto *azimuthPresetLayout = new QVBoxLayout(azimuthPresetGroup);
    m_azimuthPresets = new AnglePresets(AppSettings::Axis::Azimuth, azimuthPresetGroup);
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
    m_elevation = new AngleReading(180, m_elevationControls);
    m_elevation->setObjectName(QStringLiteral("elevationReading"));
    connect(m_elevation, &AngleReading::inputRejected, this, &ControlPanel::inputError);
    m_elevation->setAccessibleName(tr("Elevation angle"));
    connect(m_elevation, &AngleReading::angleRequested, this, [this](qint64 angle) {
        requestElevation(int(angle));
    });
    elevationColumn->addWidget(m_slider, 1, Qt::AlignHCenter);
    elevationColumn->addWidget(m_elevation);
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

    connect(m_slider, &QSlider::valueChanged, this, [this] {
        if (m_commandEnabled && m_elevationSupported)
            m_elevationCommandTimer->start();
    });
    connect(m_elevationCommandTimer, &QTimer::timeout, this, [this] {
        requestElevation(m_slider->value());
    });

    setupMotionButtons();
    const AppSettings preferences;
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
    if (!supported)
        m_elevationMotion->cancelMotion();
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
    m_azimuth->setRange(m_shortestPathEnabled ? 0 : m_azimuthScale.minimum(),
                       m_shortestPathEnabled ? 359 : m_azimuthScale.maximum(), m_shortestPathEnabled);
    m_azimuth->setInputResolver([this](const QString &text) -> AngleReading::InputResult {
        const auto result = TargetBearing::calculate(m_stationGrid, text);
        if (result.solution) {
            // NOTE: LP 只反转网格及 DXCC 算出的方位；数字输入、预设和罗盘选角不经过此解析器。
            const int shortBearing = result.solution->roundedDegrees();
            const int bearing = m_longPathEnabled ? AzimuthScale::normalized(shortBearing + 180)
                                                 : shortBearing;
            if (m_shortestPathEnabled)
                return {bearing, {}};
            const int mechanical = m_azimuthScale.mechanicalBearing(
                bearing, m_azimuthValue);
            return {m_azimuthScale.corrected(mechanical), {}};
        }
        switch (result.error) {
        case TargetBearing::Error::InvalidStationGrid:
            return {std::nullopt, tr("Set your grid in Settings → My grid… before using a grid or callsign.")};
        case TargetBearing::Error::UndefinedBearing:
            return {std::nullopt, tr("This target has no unique bearing from your grid.")};
        case TargetBearing::Error::UnknownTarget:
        case TargetBearing::Error::None:
            return {std::nullopt, tr("Unknown target. Enter a 4, 6 or 8 character grid, callsign or DXCC prefix.")};
        }
        return {};
    }, m_shortestPathEnabled
           ? tr("Click to enter an integer angle, grid, callsign or prefix. Angles wrap at 360° and use the shortest reachable rotation. Callsigns use DXCC coordinates. Press Enter to move or Esc to cancel.")
           : tr("Click to enter an angle (%1–%2°), grid, callsign or prefix. Callsigns use DXCC coordinates. Press Enter to move or Esc to cancel.")
                 .arg(m_azimuthScale.minimum()).arg(m_azimuthScale.maximum()));
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
    m_azimuth->setEditingEnabled(enabled);
    m_elevation->setEditingEnabled(enabled && m_elevationSupported);
    // NOTE: 预设编辑属于本地偏好设置，断开或透传时仍可编辑；发送端会校验真实连接状态。
    m_azimuthPresets->setEnabled(true);
    m_elevationPresets->setEnabled(true);
}

void ControlPanel::setPosition(int azimuth, int elevation)
{
    m_azimuthValue = qBound(0, azimuth, 450);
    m_elevationValue = qBound(0, elevation, 180);
    m_azimuth->setAngle(m_azimuthScale.corrected(m_azimuthValue));
    m_elevation->setAngle(m_elevationValue);
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

int ControlPanel::elevation() const
{
    return m_elevationValue;
}
