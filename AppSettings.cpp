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

#include "AppSettings.h"
#include "MaidenheadLocator.h"
#include "n1mm/N1mmProtocol.h"

#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

const QString SerialPortKey = QStringLiteral("connection/serialPort");
const QString BaudRateKey = QStringLiteral("connection/baudRate");
const QString ConnectionAddressKey = QStringLiteral("connection/address");
const QString NetworkModeKey = QStringLiteral("connection/networkMode");
const QString ConsoleVisibleKey = QStringLiteral("view/consoleVisible");
const QString WindowSizeKey = QStringLiteral("view/windowSize");
const QString AlwaysOnTopKey = QStringLiteral("view/alwaysOnTop");
const QString GreatCircleMapEnabledKey = QStringLiteral("view/greatCircleMapEnabled");
const QString AntennaGainKey = QStringLiteral("view/antennaGain");
const QString StationGridKey = QStringLiteral("station/grid");
const QString AzimuthOffsetKey = QStringLiteral("rotor/azimuthOffset");
const QString ShortestPathEnabledKey = QStringLiteral("rotor/shortestPathEnabled");
const QString WindProtectionEnabledKey = QStringLiteral("rotor/windProtectionEnabled");
const QString WindDirectionOffsetKey = QStringLiteral("rotor/windDirectionOffset");
const QString QWeatherApiHostKey = QStringLiteral("weather/qweatherApiHost");
const QString QWeatherApiKeyKey = QStringLiteral("weather/qweatherApiKey");
const QString WindWeatherProviderKey = QStringLiteral("weather/windProvider");
const QString WindWeatherTermsAcceptedKey = QStringLiteral("weather/windTermsAccepted");
const QString N1mmEnabledKey = QStringLiteral("n1mm/enabled");
const QString N1mmRotorNameKey = QStringLiteral("n1mm/rotorName");
const QString GridTrackerEnabledKey = QStringLiteral("gridtracker/enabled");
const QString WsjtXEnabledKey = QStringLiteral("wsjtx/enabled");
const QString HamAlertEnabledKey = QStringLiteral("hamalert/enabled");
const QString HamAlertUsernameKey = QStringLiteral("hamalert/username");
const QString HamAlertPasswordKey = QStringLiteral("hamalert/telnetPassword");

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

bool isValidQWeatherApiHost(const QString &host)
{
    static const QString suffix = QStringLiteral(".qweatherapi.com");
    if (host.isEmpty() || host.size() > 253 || host.startsWith(QLatin1Char('.')) ||
        host.endsWith(QLatin1Char('.')) || host.contains(QStringLiteral("..")) ||
        !host.endsWith(suffix, Qt::CaseInsensitive) || host.size() <= suffix.size())
        return false;
    static const QRegularExpression pattern(
        QStringLiteral("\\A(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\\.)+[A-Za-z]{2,63}\\z"));
    return pattern.match(host).hasMatch();
}

bool isValidWindowSize(const QSize &size)
{
    constexpr int maximumDimension = 32768;
    return size.width() > 0 && size.height() > 0 &&
           size.width() <= maximumDimension && size.height() <= maximumDimension;
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

QSize AppSettings::windowSize() const
{
    const QSize size = m_settings.value(WindowSizeKey).toSize();
    return isValidWindowSize(size) ? size : QSize(1280, 360);
}

bool AppSettings::setWindowSize(const QSize &size)
{
    return isValidWindowSize(size) && storeValue(WindowSizeKey, size);
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

double AppSettings::antennaGain() const
{
    bool ok = false;
    const double gain = m_settings.value(AntennaGainKey).toDouble(&ok);
    return ok && std::isfinite(gain) && gain >= minimumAntennaGain() &&
               gain <= maximumAntennaGain()
           ? gain
           : minimumAntennaGain();
}

bool AppSettings::setAntennaGain(double gain)
{
    return std::isfinite(gain) && gain >= minimumAntennaGain() &&
           gain <= maximumAntennaGain() && storeValue(AntennaGainKey, gain);
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

bool AppSettings::windProtectionEnabled() const
{
    return readBoolean(m_settings.value(WindProtectionEnabledKey));
}

bool AppSettings::setWindProtectionEnabled(bool enabled)
{
    return storeValue(WindProtectionEnabledKey, enabled);
}

int AppSettings::windDirectionOffset() const
{
    bool ok = false;
    const int degrees = m_settings.value(WindDirectionOffsetKey).toString().toInt(&ok);
    return ok && degrees >= 0 && degrees < 360 ? degrees : 90;
}

bool AppSettings::setWindDirectionOffset(int degrees)
{
    return degrees >= 0 && degrees < 360 && storeValue(WindDirectionOffsetKey, degrees);
}

QString AppSettings::qWeatherApiHost() const
{
    const QString host = m_settings.value(QWeatherApiHostKey).toString().trimmed();
    return isValidQWeatherApiHost(host) ? host : QString();
}

QString AppSettings::qWeatherApiKey() const
{
    const QString key = m_settings.value(QWeatherApiKeyKey).toString().trimmed();
    if (key.isEmpty() || key.size() > 512)
        return {};
    for (const QChar character : key) {
        if (character.unicode() < 0x21 || character.unicode() > 0x7e)
            return {};
    }
    return key;
}

bool AppSettings::setQWeatherCredentials(const QString &apiHost, const QString &apiKey)
{
    const QString host = apiHost.trimmed();
    const QString key = apiKey.trimmed();
    if (host.isEmpty() != key.isEmpty() ||
        (!host.isEmpty() && !isValidQWeatherApiHost(host)) ||
        (!key.isEmpty() && (key.size() > 512 ||
                            std::any_of(key.cbegin(), key.cend(), [](QChar character) {
                                return character.unicode() < 0x21 || character.unicode() > 0x7e;
                            }))))
        return false;
    m_settings.setValue(QWeatherApiHostKey, host);
    m_settings.setValue(QWeatherApiKeyKey, key);
    m_settings.sync();
    return m_settings.status() == QSettings::NoError;
}

WindWeather::Provider AppSettings::windWeatherProvider() const
{
    if (!m_settings.contains(WindWeatherProviderKey)) {
        if (!qWeatherApiHost().isEmpty() && !qWeatherApiKey().isEmpty())
            return WindWeather::Provider::QWeather;
        if (windProtectionEnabled())
            return WindWeather::Provider::QWeather;
        return WindWeather::Provider::OpenMeteo;
    }
    const QString provider = m_settings.value(WindWeatherProviderKey).toString();
    if (provider == QStringLiteral("qweather"))
        return WindWeather::Provider::QWeather;
    if (provider == QStringLiteral("met-norway"))
        return WindWeather::Provider::MetNorway;
    return WindWeather::Provider::OpenMeteo;
}

bool AppSettings::windWeatherTermsAccepted() const
{
    return readBoolean(m_settings.value(WindWeatherTermsAcceptedKey));
}

bool AppSettings::setWindWeatherProvider(WindWeather::Provider provider, bool termsAccepted)
{
    switch (provider) {
    case WindWeather::Provider::OpenMeteo:
        break;
    case WindWeather::Provider::QWeather:
        termsAccepted = false;
        break;
    case WindWeather::Provider::MetNorway:
        break;
    default:
        return false;
    }
    m_settings.setValue(WindWeatherProviderKey,
                        provider == WindWeather::Provider::QWeather
                            ? QStringLiteral("qweather")
                            : provider == WindWeather::Provider::MetNorway
                                ? QStringLiteral("met-norway")
                                : QStringLiteral("open-meteo"));
    m_settings.setValue(WindWeatherTermsAcceptedKey, termsAccepted);
    m_settings.sync();
    return m_settings.status() == QSettings::NoError;
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

bool AppSettings::wsjtXEnabled() const
{
    return readBoolean(m_settings.value(WsjtXEnabledKey));
}

bool AppSettings::setWsjtXEnabled(bool enabled)
{
    return storeValue(WsjtXEnabledKey, enabled);
}

QString AppSettings::hamAlertUsername() const
{
    const QString username = m_settings.value(HamAlertUsernameKey).toString().trimmed();
    if (username.isEmpty() || username.size() > 64)
        return {};
    for (const QChar character : username) {
        if (character.unicode() < 0x21 || character.unicode() > 0x7e)
            return {};
    }
    return username;
}

QString AppSettings::hamAlertTelnetPassword() const
{
    const QString password = m_settings.value(HamAlertPasswordKey).toString().trimmed();
    if (password.isEmpty() || password.size() > 256)
        return {};
    for (const QChar character : password) {
        if (character.unicode() < 0x20 || character.unicode() > 0x7e)
            return {};
    }
    return password;
}

bool AppSettings::setHamAlertCredentials(const QString &username, const QString &password)
{
    const QString normalizedUsername = username.trimmed();
    const QString normalizedPassword = password.trimmed();
    if (normalizedUsername.isEmpty() || normalizedUsername.size() > 64 ||
        normalizedPassword.isEmpty() || normalizedPassword.size() > 256)
        return false;
    for (const QChar character : normalizedUsername) {
        if (character.unicode() < 0x21 || character.unicode() > 0x7e)
            return false;
    }
    for (const QChar character : normalizedPassword) {
        if (character.unicode() < 0x20 || character.unicode() > 0x7e)
            return false;
    }

    m_settings.setValue(HamAlertUsernameKey, normalizedUsername);
    m_settings.setValue(HamAlertPasswordKey, normalizedPassword);
    m_settings.sync();
    return m_settings.status() == QSettings::NoError;
}

bool AppSettings::hamAlertEnabled() const
{
    return readBoolean(m_settings.value(HamAlertEnabledKey));
}

bool AppSettings::setHamAlertEnabled(bool enabled)
{
    return storeValue(HamAlertEnabledKey, enabled);
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
