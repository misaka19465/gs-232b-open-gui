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

#include "weather/WindProtectionController.h"

#include <QJsonDocument>
#include <QtTest>

class WindProtectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesCurrentWindDirection_data();
    void parsesCurrentWindDirection();
    void parsesOpenMeteoWindDirection();
    void parsesMetNorwayWindDirection();
    void rejectsMalformedProviderResponses();
    void roundsFractionalDirections();
    void rejectsInvalidWindDirection_data();
    void rejectsInvalidWindDirection();
    void calculatesTargetBearingAndAngularDifference();
    void appliesRotationThreshold();
    void convertsCoordinatesForQWeather();
};

void WindProtectionTests::parsesCurrentWindDirection_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<int>("expectedDirection");
    QTest::newRow("north")
        << QByteArray(R"({"metadata":{"attributions":["https://developer.qweather.com/attribution.html"]},"validTime":"2026-11-19T12:00+08:00","wind":{"direction":{"degree":0,"compass":"n"}}})")
        << 0;
    QTest::newRow("maximum")
        << QByteArray(R"({"validTime":"2026-11-19T12:00+08:00","wind":{"direction":{"degree":359,"compass":"nnw"}}})")
        << 359;
    QTest::newRow("north-360")
        << QByteArray(R"({"validTime":"2026-11-19T12:00+08:00","wind":{"direction":{"degree":360,"compass":"n"}}})")
        << 0;
    QTest::newRow("fractional")
        << QByteArray(R"({"validTime":"2026-11-19T12:00+08:00","wind":{"direction":{"degree":1.5,"compass":"n"}}})")
        << 2;
    QTest::newRow("east")
        << QByteArray(R"({"validTime":"2026-11-19T12:00+08:00","wind":{"direction":{"degree":90,"compass":"E"}}})")
        << 90;
}

void WindProtectionTests::parsesCurrentWindDirection()
{
    QFETCH(QByteArray, payload);
    QFETCH(int, expectedDirection);
    const auto observation = WindProtection::parseQWeatherResponse(payload);
    QVERIFY(observation.has_value());
    QCOMPARE(observation->direction, expectedDirection);
    QCOMPARE(observation->validTime, QStringLiteral("2026-11-19T12:00+08:00"));
}

void WindProtectionTests::parsesOpenMeteoWindDirection()
{
    const auto observation = WindProtection::parseOpenMeteoResponse(
        QByteArray(R"({"current":{"time":"2026-11-19T12:00","wind_direction_10m":360}})"));
    QVERIFY(observation.has_value());
    QCOMPARE(observation->direction, 0);
    QCOMPARE(observation->validTime, QStringLiteral("2026-11-19T12:00"));
}

void WindProtectionTests::parsesMetNorwayWindDirection()
{
    const auto observation = WindProtection::parseMetNorwayResponse(QByteArray(
        R"({"properties":{"timeseries":[{"time":"2026-11-19T12:00:00Z","data":{"instant":{"details":{"wind_from_direction":247.5}}}}]}})"));
    QVERIFY(observation.has_value());
    QCOMPARE(observation->direction, 248);
    QCOMPARE(observation->validTime, QStringLiteral("2026-11-19T12:00:00Z"));
}

void WindProtectionTests::rejectsMalformedProviderResponses()
{
    QVERIFY(!WindProtection::parseOpenMeteoResponse(
                 QByteArray(R"({"current":{"wind_direction_10m":null}})"))
                 .has_value());
    QVERIFY(!WindProtection::parseMetNorwayResponse(
                 QByteArray(R"({"properties":{"timeseries":[]}})"))
                 .has_value());
    QVERIFY(!WindProtection::parseMetNorwayResponse(QByteArray(
                 R"({"properties":{"timeseries":[{"data":{"instant":{"details":{"wind_from_direction":"90"}}}}]}})"))
                 .has_value());
}

void WindProtectionTests::roundsFractionalDirections()
{
    const auto observation = WindProtection::parseOpenMeteoResponse(
        QByteArray(R"({"current":{"wind_direction_10m":359.6}})"));
    QVERIFY(observation.has_value());
    QCOMPARE(observation->direction, 0);
}

void WindProtectionTests::rejectsInvalidWindDirection_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::newRow("invalid-json") << QByteArray("{");
    QTest::newRow("empty") << QByteArray("{}");
    QTest::newRow("api-error")
        << QByteArray(R"({"code":"403","wind":{"direction":{"degree":0,"compass":"n"}}})");
    QTest::newRow("no-direction")
        << QByteArray(R"({"wind":{"direction":{"compass":"none"}}})");
    QTest::newRow("variable-direction")
        << QByteArray(R"({"wind":{"direction":{"degree":365,"compass":"VRB"}}})");
    QTest::newRow("string-degree")
        << QByteArray(R"({"wind":{"direction":{"degree":"90","compass":"e"}}})");
}

void WindProtectionTests::rejectsInvalidWindDirection()
{
    QFETCH(QByteArray, payload);
    QVERIFY(!WindProtection::parseQWeatherResponse(payload).has_value());
}

void WindProtectionTests::calculatesTargetBearingAndAngularDifference()
{
    QCOMPARE(WindProtection::targetBearing(0, 90), 90);
    QCOMPARE(WindProtection::targetBearing(350, 90), 80);
    QCOMPARE(WindProtection::targetBearing(10, 270), 280);
    QCOMPARE(WindProtection::angularDifference(355, 5), 10);
    QCOMPARE(WindProtection::angularDifference(10, 350), 20);
}

void WindProtectionTests::appliesRotationThreshold()
{
    QVERIFY(!WindProtection::requiresRotation(350, 4));
    QVERIFY(!WindProtection::requiresRotation(0, 14));
    QVERIFY(WindProtection::requiresRotation(0, 15));
    QVERIFY(WindProtection::requiresRotation(180, 0));
}

void WindProtectionTests::convertsCoordinatesForQWeather()
{
    const QPointF beijing = WindProtection::qWeatherCoordinates(QPointF(116.41, 39.92));
    QVERIFY(qAbs(beijing.x() - 116.416) < 0.001);
    QVERIFY(qAbs(beijing.y() - 39.922) < 0.001);

    const QPointF tokyo(139.69, 35.68);
    QCOMPARE(WindProtection::qWeatherCoordinates(tokyo), tokyo);
}

QTEST_GUILESS_MAIN(WindProtectionTests)
#include "WindProtectionTests.moc"
