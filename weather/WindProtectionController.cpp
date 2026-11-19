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

#include "WindProtectionController.h"

#include "MaidenheadLocator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <cmath>

namespace {

constexpr qint64 MaximumResponseBytes = 64 * 1024;
constexpr double Pi = 3.14159265358979323846;
constexpr double EarthSemiMajorAxis = 6378245.0;
constexpr double EarthEccentricitySquared = 0.00669342162296594323;

int normalizedAngle(qint64 angle)
{
    return int((angle % 360 + 360) % 360);
}

bool readDirection(const QJsonValue &value, int &direction)
{
    const bool ok = value.isDouble();
    const double number = value.toDouble(-1);
    if (!ok || !std::isfinite(number) || number < 0 || number > 360)
        return false;
    direction = normalizedAngle(qRound(number));
    return true;
}

double transformLatitude(double longitude, double latitude)
{
    double result = -100.0 + 2.0 * longitude + 3.0 * latitude +
                    0.2 * latitude * latitude + 0.1 * longitude * latitude +
                    0.2 * std::sqrt(std::abs(longitude));
    result += (20.0 * std::sin(6.0 * longitude * Pi) +
               20.0 * std::sin(2.0 * longitude * Pi)) * 2.0 / 3.0;
    result += (20.0 * std::sin(latitude * Pi) +
               40.0 * std::sin(latitude * Pi / 3.0)) * 2.0 / 3.0;
    result += (160.0 * std::sin(latitude * Pi / 12.0) +
               320.0 * std::sin(latitude * Pi / 30.0)) * 2.0 / 3.0;
    return result;
}

double transformLongitude(double longitude, double latitude)
{
    double result = 300.0 + longitude + 2.0 * latitude +
                    0.1 * longitude * longitude + 0.1 * longitude * latitude +
                    0.1 * std::sqrt(std::abs(longitude));
    result += (20.0 * std::sin(6.0 * longitude * Pi) +
               20.0 * std::sin(2.0 * longitude * Pi)) * 2.0 / 3.0;
    result += (20.0 * std::sin(longitude * Pi) +
               40.0 * std::sin(longitude * Pi / 3.0)) * 2.0 / 3.0;
    result += (150.0 * std::sin(longitude * Pi / 12.0) +
               300.0 * std::sin(longitude * Pi / 30.0)) * 2.0 / 3.0;
    return result;
}

} // namespace

std::optional<WindProtection::Observation> WindProtection::parseQWeatherResponse(
    const QByteArray &payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("code")) &&
        root.value(QStringLiteral("code")).toVariant().toString() != QStringLiteral("200"))
        return std::nullopt;
    const QJsonObject wind = root.value(QStringLiteral("wind")).toObject();
    const QJsonObject directionData = wind.value(QStringLiteral("direction")).toObject();
    const QString compass = directionData.value(QStringLiteral("compass")).toString().toLower();
    if (compass == QStringLiteral("none") || compass == QStringLiteral("vrb"))
        return std::nullopt;
    int direction = 0;
    if (!readDirection(directionData.value(QStringLiteral("degree")), direction))
        return std::nullopt;

    return Observation{direction, root.value(QStringLiteral("validTime")).toString()};
}

std::optional<WindProtection::Observation> WindProtection::parseOpenMeteoResponse(
    const QByteArray &payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("error")).toBool())
        return std::nullopt;
    const QJsonObject current = root.value(QStringLiteral("current")).toObject();
    int direction = 0;
    if (!readDirection(current.value(QStringLiteral("wind_direction_10m")), direction))
        return std::nullopt;

    return Observation{direction, current.value(QStringLiteral("time")).toString()};
}

std::optional<WindProtection::Observation> WindProtection::parseMetNorwayResponse(
    const QByteArray &payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    const QJsonObject properties = document.object()
                                       .value(QStringLiteral("properties")).toObject();
    const QJsonArray timeSeries = properties.value(QStringLiteral("timeseries")).toArray();
    if (timeSeries.isEmpty())
        return std::nullopt;

    const QJsonObject firstEntry = timeSeries.first().toObject();
    const QJsonObject instant = firstEntry.value(QStringLiteral("data")).toObject()
                                    .value(QStringLiteral("instant")).toObject();
    const QJsonObject details = instant.value(QStringLiteral("details")).toObject();
    int direction = 0;
    if (!readDirection(details.value(QStringLiteral("wind_from_direction")), direction))
        return std::nullopt;

    return Observation{direction, firstEntry.value(QStringLiteral("time")).toString()};
}

int WindProtection::targetBearing(int windDirection, int antennaOffset)
{
    return normalizedAngle(qint64(windDirection) + antennaOffset);
}

int WindProtection::angularDifference(int first, int second)
{
    const int difference = qAbs(normalizedAngle(first) - normalizedAngle(second));
    return qMin(difference, 360 - difference);
}

bool WindProtection::requiresRotation(int currentBearing, int targetBearing, int threshold)
{
    return threshold > 0 && angularDifference(currentBearing, targetBearing) >= threshold;
}

QPointF WindProtection::qWeatherCoordinates(const QPointF &wgs84LongitudeLatitude)
{
    const double longitude = wgs84LongitudeLatitude.x();
    const double latitude = wgs84LongitudeLatitude.y();
    // NOTE: GCJ-02 常用范围是近似矩形；范围外保持 WGS-84，覆盖和风天气的全球坐标约定。
    if (longitude < 72.004 || longitude > 137.8347 ||
        latitude < 0.8293 || latitude > 55.8271)
        return wgs84LongitudeLatitude;

    const double x = longitude - 105.0;
    const double y = latitude - 35.0;
    const double latitudeDelta = transformLatitude(x, y);
    const double longitudeDelta = transformLongitude(x, y);
    const double latitudeRadians = latitude / 180.0 * Pi;
    double magic = std::sin(latitudeRadians);
    magic = 1.0 - EarthEccentricitySquared * magic * magic;
    const double squareRootMagic = std::sqrt(magic);
    const double adjustedLatitude = latitudeDelta * 180.0 /
        ((EarthSemiMajorAxis * (1.0 - EarthEccentricitySquared) /
          (magic * squareRootMagic)) * Pi);
    const double adjustedLongitude = longitudeDelta * 180.0 /
        ((EarthSemiMajorAxis / squareRootMagic * std::cos(latitudeRadians)) * Pi);
    return QPointF(longitude + adjustedLongitude, latitude + adjustedLatitude);
}

WindProtectionController::WindProtectionController(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)), m_pollTimer(new QTimer(this))
{
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString cacheDirectory = QDir(cacheRoot).filePath(QStringLiteral("wind-weather"));
    if (!cacheRoot.isEmpty() && QDir().mkpath(cacheDirectory) &&
        QFileInfo(cacheDirectory).isWritable()) {
        auto *cache = new QNetworkDiskCache(m_network);
        cache->setCacheDirectory(cacheDirectory);
        cache->setMaximumCacheSize(2 * 1024 * 1024);
        m_network->setCache(cache);
    }
    m_pollTimer->setInterval(PollIntervalMilliseconds);
    connect(m_pollTimer, &QTimer::timeout, this,
            &WindProtectionController::requestWindDirection);
}

void WindProtectionController::setWeatherConfiguration(WindWeather::Provider provider,
                                                        const QString &apiHost,
                                                        const QString &apiKey,
                                                        bool termsAccepted)
{
    if (m_provider == provider && m_apiHost == apiHost && m_apiKey == apiKey &&
        m_providerTermsAccepted == termsAccepted) {
        if (m_enabled && m_connected && !m_activeReply && !m_pollTimer->isActive()) {
            m_pollTimer->setInterval(pollIntervalMilliseconds());
            m_pollTimer->start();
            requestWindDirection();
        }
        return;
    }
    m_provider = provider;
    m_apiHost = apiHost;
    m_apiKey = apiKey;
    m_providerTermsAccepted = termsAccepted;
    m_windDirection = -1;
    m_lastRequestedTarget = -1;
    emit windDirectionChanged(-1);
    if (m_enabled && m_connected) {
        abortActiveRequest();
        m_pollTimer->setInterval(pollIntervalMilliseconds());
        m_pollTimer->start();
        requestWindDirection();
    }
}

void WindProtectionController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    updateActiveState();
}

void WindProtectionController::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    m_hasPosition = false;
    m_windDirection = -1;
    m_lastRequestedTarget = -1;
    updateActiveState();
}

void WindProtectionController::setStationGrid(const QString &grid)
{
    const QString normalized = grid.trimmed().toUpper();
    if (m_stationGrid == normalized)
        return;
    m_stationGrid = normalized;
    m_windDirection = -1;
    m_lastRequestedTarget = -1;
    if (m_enabled && m_connected) {
        abortActiveRequest();
        m_pollTimer->setInterval(pollIntervalMilliseconds());
        m_pollTimer->start();
        requestWindDirection();
    }
}

void WindProtectionController::setDirectionOffset(int degrees)
{
    if (degrees < 0 || degrees >= 360 || m_directionOffset == degrees)
        return;
    m_directionOffset = degrees;
    m_lastRequestedTarget = -1;
    evaluateTarget();
}

void WindProtectionController::setAzimuthOffset(int degrees)
{
    if (m_azimuthOffset == degrees)
        return;
    m_azimuthOffset = degrees;
    m_lastRequestedTarget = -1;
    evaluateTarget();
}

void WindProtectionController::setCurrentAzimuth(int mechanicalAzimuth)
{
    m_mechanicalAzimuth = mechanicalAzimuth;
    m_hasPosition = m_connected;
    evaluateTarget();
}

bool WindProtectionController::configurationReady() const
{
    if (!MaidenheadLocator::isValid(m_stationGrid))
        return false;
    if (m_provider == WindWeather::Provider::QWeather)
        return !m_apiHost.isEmpty() && !m_apiKey.isEmpty();
    return m_providerTermsAccepted &&
           (m_provider != WindWeather::Provider::MetNorway || m_network->cache());
}

int WindProtectionController::pollIntervalMilliseconds() const
{
    return m_provider == WindWeather::Provider::MetNorway
        ? GlobalForecastPollIntervalMilliseconds
        : PollIntervalMilliseconds;
}

void WindProtectionController::updateActiveState()
{
    const bool active = m_enabled && m_connected;
    if (!active) {
        m_pollTimer->stop();
        abortActiveRequest();
        m_hasPosition = false;
        m_windDirection = -1;
        m_lastRequestedTarget = -1;
        return;
    }

    if (!m_pollTimer->isActive())
        m_pollTimer->setInterval(pollIntervalMilliseconds());
    if (!m_pollTimer->isActive())
        m_pollTimer->start();
    requestWindDirection();
}

void WindProtectionController::requestWindDirection()
{
    if (!m_enabled || !m_connected || m_activeReply)
        return;
    if (m_provider == WindWeather::Provider::QWeather &&
        (m_apiHost.isEmpty() || m_apiKey.isEmpty())) {
        m_pollTimer->stop();
        emit statusChanged(tr("Configure the QWeather API Host and API Key in Settings."));
        return;
    }
    if (!MaidenheadLocator::isValid(m_stationGrid)) {
        m_pollTimer->stop();
        emit statusChanged(tr("Set your Maidenhead grid in Settings → My grid… before using wind protection."));
        return;
    }
    if (m_provider != WindWeather::Provider::QWeather && !m_providerTermsAccepted) {
        m_pollTimer->stop();
        emit statusChanged(tr("Review and accept the selected weather provider terms in Settings."));
        return;
    }
    if (m_provider == WindWeather::Provider::MetNorway && !m_network->cache()) {
        m_pollTimer->stop();
        emit statusChanged(tr("MET Norway requires a writable HTTP cache to honor response expiry."));
        return;
    }
    const auto gridCenter = MaidenheadLocator::center(m_stationGrid);
    if (!gridCenter) {
        emit statusChanged(tr("The configured Maidenhead grid is invalid."));
        return;
    }

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    if (m_provider == WindWeather::Provider::QWeather) {
        const QPointF coordinates = WindProtection::qWeatherCoordinates(*gridCenter);
        const QString latitude = QString::number(coordinates.y(), 'f', 2);
        const QString longitude = QString::number(coordinates.x(), 'f', 2);
        url.setHost(m_apiHost);
        url.setPath(QStringLiteral("/weather/v1/current/%1/%2").arg(latitude, longitude));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("lang"), QStringLiteral("en"));
        url.setQuery(query);
    } else if (m_provider == WindWeather::Provider::OpenMeteo) {
        url.setHost(QStringLiteral("api.open-meteo.com"));
        url.setPath(QStringLiteral("/v1/forecast"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("latitude"), QString::number(gridCenter->y(), 'f', 4));
        query.addQueryItem(QStringLiteral("longitude"), QString::number(gridCenter->x(), 'f', 4));
        query.addQueryItem(QStringLiteral("current"), QStringLiteral("wind_direction_10m"));
        query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
        url.setQuery(query);
    } else {
        url.setHost(QStringLiteral("api.met.no"));
        url.setPath(QStringLiteral("/weatherapi/locationforecast/2.0/compact"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("lat"), QString::number(gridCenter->y(), 'f', 4));
        query.addQueryItem(QStringLiteral("lon"), QString::number(gridCenter->x(), 'f', 4));
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    if (m_provider == WindWeather::Provider::QWeather)
        request.setRawHeader("X-QW-Api-Key", m_apiKey.toUtf8());
    else if (m_provider == WindWeather::Provider::MetNorway) {
        request.setRawHeader("User-Agent",
            "GS-232B-Open-Control (https://github.com/misaka19465/gs-232b-open-gui)");
        request.setRawHeader("Accept", "application/geo+json, application/json");
    }
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferNetwork);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    m_responseTooLarge = false;
    QNetworkReply *reply = m_network->get(request);
    m_activeReply = reply;
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        if (m_activeReply == reply && reply->bytesAvailable() > MaximumResponseBytes) {
            m_responseTooLarge = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_activeReply != reply) {
            reply->deleteLater();
            return;
        }
        m_activeReply.clear();
        reply->deleteLater();
        if (!m_enabled || !m_connected)
            return;

        if (m_responseTooLarge) {
            emit statusChanged(tr("Weather API response exceeded the size limit."));
            return;
        }
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300) {
            if (httpStatus >= 400 && httpStatus < 500 && httpStatus != 429) {
                m_pollTimer->stop();
            } else if (httpStatus == 429) {
                m_pollTimer->setInterval(qMax(pollIntervalMilliseconds(), 30 * 60 * 1000));
                m_pollTimer->start();
            }
            emit statusChanged(httpStatus > 0
                                   ? tr("Weather API request failed (HTTP %1).").arg(httpStatus)
                                   : tr("Weather API request failed: %1").arg(reply->errorString()));
            return;
        }

        const bool providerMaintenance = m_provider == WindWeather::Provider::MetNorway &&
                                         httpStatus == 203;
        const QByteArray payload = reply->readAll();
        std::optional<WindProtection::Observation> observation;
        switch (m_provider) {
        case WindWeather::Provider::OpenMeteo:
            observation = WindProtection::parseOpenMeteoResponse(payload);
            break;
        case WindWeather::Provider::MetNorway:
            observation = WindProtection::parseMetNorwayResponse(payload);
            break;
        case WindWeather::Provider::QWeather:
            observation = WindProtection::parseQWeatherResponse(payload);
            break;
        }
        if (!observation) {
            emit statusChanged(tr("Weather API returned invalid wind direction data."));
            return;
        }

        m_windDirection = observation->direction;
        m_pollTimer->setInterval(pollIntervalMilliseconds());
        m_pollTimer->start();
        m_lastRequestedTarget = -1;
        emit windDirectionChanged(m_windDirection);
        emit statusChanged(providerMaintenance
                               ? tr("MET Norway returned HTTP 203; check for a newer app version.")
                               : tr("Wind from %1°; checking the configured antenna angle.")
                                     .arg(m_windDirection));
        evaluateTarget();
    });
}

void WindProtectionController::abortActiveRequest()
{
    if (!m_activeReply)
        return;
    QNetworkReply *reply = m_activeReply;
    m_activeReply.clear();
    reply->abort();
}

void WindProtectionController::evaluateTarget()
{
    if (!m_enabled || !m_connected || !m_hasPosition || m_windDirection < 0)
        return;

    const int target = WindProtection::targetBearing(m_windDirection, m_directionOffset);
    const int currentBearing = normalizedAngle(qint64(m_mechanicalAzimuth) + m_azimuthOffset);
    if (!WindProtection::requiresRotation(currentBearing, target,
                                          RotationThresholdDegrees)) {
        m_lastRequestedTarget = -1;
        return;
    }
    if (m_lastRequestedTarget == target)
        return;

    m_lastRequestedTarget = target;
    emit targetBearingRequested(target);
}
