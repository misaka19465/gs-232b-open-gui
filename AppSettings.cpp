#include "AppSettings.h"
#include "MaidenheadLocator.h"
#include "n1mm/N1mmProtocol.h"

#include <array>

namespace {

const QString SerialPortKey = QStringLiteral("connection/serialPort");
const QString BaudRateKey = QStringLiteral("connection/baudRate");
const QString ConnectionAddressKey = QStringLiteral("connection/address");
const QString NetworkModeKey = QStringLiteral("connection/networkMode");
const QString ConsoleVisibleKey = QStringLiteral("view/consoleVisible");
const QString AlwaysOnTopKey = QStringLiteral("view/alwaysOnTop");
const QString GreatCircleMapEnabledKey = QStringLiteral("view/greatCircleMapEnabled");
const QString StationGridKey = QStringLiteral("station/grid");
const QString AzimuthOffsetKey = QStringLiteral("rotor/azimuthOffset");
const QString ShortestPathEnabledKey = QStringLiteral("rotor/shortestPathEnabled");
const QString N1mmEnabledKey = QStringLiteral("n1mm/enabled");
const QString N1mmRotorNameKey = QStringLiteral("n1mm/rotorName");
const QString GridTrackerEnabledKey = QStringLiteral("gridtracker/enabled");

bool isSupportedBaudRate(int baudRate)
{
    return baudRate == 1200 || baudRate == 2400 || baudRate == 4800 || baudRate == 9600;
}

bool readBoolean(const QVariant &stored, bool fallback = false)
{
    // NOTE: INI 重启后布尔值可能作为字符串读取，仅识别明确布尔值，其余使用该偏好的默认值。
    const QString value = stored.toString();
    if (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || value == QStringLiteral("1"))
        return true;
    if (value.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0 || value == QStringLiteral("0"))
        return false;
    return fallback;
}

QString presetKey(AppSettings::Axis axis, int index)
{
    // NOTE: 预设键沿用既有格式，与控件 objectName 解耦后仍可直接读取用户数据。
    const QString prefix = axis == AppSettings::Axis::Azimuth
                               ? QStringLiteral("azimuthPreset")
                               : QStringLiteral("elevationPreset");
    return prefix + QString::number(index);
}

int maximumAngle(AppSettings::Axis axis)
{
    return axis == AppSettings::Axis::Azimuth ? 450 : 180;
}

int defaultAngle(AppSettings::Axis axis, int index)
{
    constexpr std::array<int, AppSettings::presetCount(AppSettings::Axis::Elevation)> elevation{
        0, 25, 50, 75, 100, 125};
    return axis == AppSettings::Axis::Azimuth ? index * 45 : elevation[index];
}

} // namespace

AppSettings::AppSettings()
    // NOTE: 保留既有组织名、应用名和原生存储格式，集中读写无需迁移已保存的预设。
    : m_settings(QSettings::NativeFormat, QSettings::UserScope,
                 QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"))
{
}

AppSettings::AppSettings(const QString &fileName)
    : m_settings(fileName, QSettings::IniFormat)
{
}

QString AppSettings::serialPort(const QStringList &availablePorts) const
{
    const QString savedPort = m_settings.value(SerialPortKey).toString().trimmed();
    return availablePorts.contains(savedPort) ? savedPort : availablePorts.value(0);
}

bool AppSettings::setSerialPort(const QString &port)
{
    return storeValue(SerialPortKey, port.trimmed());
}

int AppSettings::baudRate() const
{
    bool ok = false;
    const int baudRate = m_settings.value(BaudRateKey).toString().toInt(&ok);
    return ok && isSupportedBaudRate(baudRate) ? baudRate : 9600;
}

bool AppSettings::setBaudRate(int baudRate)
{
    return isSupportedBaudRate(baudRate) && storeValue(BaudRateKey, baudRate);
}

QString AppSettings::connectionAddress() const
{
    return m_settings.value(ConnectionAddressKey, QStringLiteral("192.168.1.50"))
        .toString().trimmed();
}

bool AppSettings::setConnectionAddress(const QString &address)
{
    return storeValue(ConnectionAddressKey, address.trimmed());
}

bool AppSettings::networkMode() const
{
    return readBoolean(m_settings.value(NetworkModeKey));
}

bool AppSettings::setNetworkMode(bool enabled)
{
    return storeValue(NetworkModeKey, enabled);
}

bool AppSettings::consoleVisible() const
{
    return readBoolean(m_settings.value(ConsoleVisibleKey));
}

bool AppSettings::setConsoleVisible(bool visible)
{
    return storeValue(ConsoleVisibleKey, visible);
}

bool AppSettings::alwaysOnTop() const
{
    return readBoolean(m_settings.value(AlwaysOnTopKey));
}

bool AppSettings::setAlwaysOnTop(bool enabled)
{
    return storeValue(AlwaysOnTopKey, enabled);
}

bool AppSettings::greatCircleMapEnabled() const
{
    return readBoolean(m_settings.value(GreatCircleMapEnabledKey));
}

bool AppSettings::setGreatCircleMapEnabled(bool enabled)
{
    return storeValue(GreatCircleMapEnabledKey, enabled);
}

int AppSettings::azimuthOffset() const
{
    bool ok = false;
    const int offset = m_settings.value(AzimuthOffsetKey).toString().toInt(&ok);
    return ok ? offset : 0;
}

bool AppSettings::setAzimuthOffset(int degrees)
{
    return storeValue(AzimuthOffsetKey, degrees);
}

bool AppSettings::shortestPathEnabled() const
{
    return readBoolean(m_settings.value(ShortestPathEnabledKey), true);
}

bool AppSettings::setShortestPathEnabled(bool enabled)
{
    return storeValue(ShortestPathEnabledKey, enabled);
}

bool AppSettings::n1mmEnabled() const
{
    return readBoolean(m_settings.value(N1mmEnabledKey));
}

bool AppSettings::setN1mmEnabled(bool enabled)
{
    return storeValue(N1mmEnabledKey, enabled);
}

QString AppSettings::n1mmRotorName() const
{
    const QString name = m_settings.value(N1mmRotorNameKey).toString();
    return N1mm::isValidRotorName(name) ? name.trimmed() : QStringLiteral("GS232B");
}

bool AppSettings::setN1mmRotorName(const QString &name)
{
    return N1mm::isValidRotorName(name) && storeValue(N1mmRotorNameKey, name.trimmed());
}

bool AppSettings::gridTrackerEnabled() const
{
    return readBoolean(m_settings.value(GridTrackerEnabledKey));
}

bool AppSettings::setGridTrackerEnabled(bool enabled)
{
    return storeValue(GridTrackerEnabledKey, enabled);
}

QString AppSettings::stationGrid() const
{
    const QString grid = m_settings.value(StationGridKey).toString().trimmed();
    return isValidStationGrid(grid) ? grid.toUpper() : QString();
}

bool AppSettings::setStationGrid(const QString &grid)
{
    const QString trimmed = grid.trimmed();
    if (!trimmed.isEmpty() && !isValidStationGrid(trimmed))
        return false;

    return storeValue(StationGridKey, trimmed.toUpper());
}

bool AppSettings::isValidStationGrid(const QString &grid)
{
    return MaidenheadLocator::isValid(grid);
}

int AppSettings::presetAngle(Axis axis, int index) const
{
    if (index < 0 || index >= presetCount(axis))
        return 0;

    bool ok = false;
    const int angle = m_settings.value(presetKey(axis, index)).toString().toInt(&ok);
    return ok && angle >= 0 && angle <= maximumAngle(axis) ? angle : defaultAngle(axis, index);
}

bool AppSettings::setPresetAngle(Axis axis, int index, int angle)
{
    if (index < 0 || index >= presetCount(axis) || angle < 0 || angle > maximumAngle(axis))
        return false;

    return storeValue(presetKey(axis, index), angle);
}

bool AppSettings::storeValue(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    // NOTE: 偏好在交互时立即落盘，不依赖正常退出；保留 QSettings 的原子写入机制。
    m_settings.sync();
    return m_settings.status() == QSettings::NoError;
}
