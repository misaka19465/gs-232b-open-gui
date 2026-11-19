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

#pragma once

#include "WindWeatherProvider.h"

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QString>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace WindProtection {

struct Observation final
{
    int direction = 0;
    QString validTime;
};

/** @brief 解析和风天气当前天气响应中的气象风向（风从哪里来）。 */
std::optional<Observation> parseQWeatherResponse(const QByteArray &payload);
/** @brief 解析 Open-Meteo 当前模型风向响应。 */
std::optional<Observation> parseOpenMeteoResponse(const QByteArray &payload);
/** @brief 解析 MET Norway 当前预报时间点的风向响应。 */
std::optional<Observation> parseMetNorwayResponse(const QByteArray &payload);
/** @brief 将气象风向和天线相对偏移换算为目标方位。 */
int targetBearing(int windDirection, int antennaOffset);
/** @brief 返回两个圆周角之间的最小夹角。 */
int angularDifference(int first, int second);
/** @brief 角差达到阈值时返回 true；默认使用抗风模式的 15° 阈值。 */
bool requiresRotation(int currentBearing, int targetBearing,
                      int threshold = 15);
/** @brief 按和风天气坐标约定将 WGS-84 网格中心转换为请求坐标。 */
QPointF qWeatherCoordinates(const QPointF &wgs84LongitudeLatitude);

} // namespace WindProtection

/** @brief 轮询所选天气源的当前风向，并在本站旋转器偏差较大时请求方位定位。 */
class WindProtectionController final : public QObject
{
    Q_OBJECT

public:
    static constexpr int PollIntervalMilliseconds = 15 * 60 * 1000;
    static constexpr int GlobalForecastPollIntervalMilliseconds = 3 * 60 * 60 * 1000;
    static constexpr int RotationThresholdDegrees = 15;

    explicit WindProtectionController(QObject *parent = nullptr);

    void setWeatherConfiguration(WindWeather::Provider provider,
                                 const QString &apiHost, const QString &apiKey,
                                 bool termsAccepted);
    void setEnabled(bool enabled);
    void setConnected(bool connected);
    void setStationGrid(const QString &grid);
    void setDirectionOffset(int degrees);
    void setAzimuthOffset(int degrees);
    void setCurrentAzimuth(int mechanicalAzimuth);
    bool configurationReady() const;

signals:
    void targetBearingRequested(int bearing);
    void windDirectionChanged(int direction);
    void statusChanged(const QString &message);

private:
    void updateActiveState();
    void requestWindDirection();
    void abortActiveRequest();
    void evaluateTarget();
    int pollIntervalMilliseconds() const;

    QNetworkAccessManager *m_network;
    QTimer *m_pollTimer;
    QPointer<QNetworkReply> m_activeReply;
    WindWeather::Provider m_provider = WindWeather::Provider::OpenMeteo;
    QString m_apiHost;
    QString m_apiKey;
    QString m_stationGrid;
    int m_directionOffset = 90;
    int m_azimuthOffset = 0;
    int m_mechanicalAzimuth = 0;
    int m_windDirection = -1;
    int m_lastRequestedTarget = -1;
    bool m_enabled = false;
    bool m_connected = false;
    bool m_hasPosition = false;
    bool m_providerTermsAccepted = false;
    bool m_responseTooLarge = false;
};
