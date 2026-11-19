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

#include "targeting/DxccDatabase.h"
#include "targeting/TargetBearing.h"

#include <QTest>

#include <limits>

class TargetBearingTests final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesGrids_data();
    void resolvesGrids();
    void resolvesDxcc_data();
    void resolvesDxcc();
    void rejectsUnknownTargets();
    void bearings_data();
    void bearings();
    void rejectsUndefinedBearings();
    void calculatesFromStationGrid();
    void reportsErrors();
};

void TargetBearingTests::resolvesGrids_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QPointF>("coordinates");
    QTest::newRow("four") << QStringLiteral("FN31") << QPointF(-73, 41.5);
    QTest::newRow("six") << QStringLiteral(" pm95aa ")
                         << QPointF(138.04166666666666, 35.020833333333336);
    QTest::newRow("eight") << QStringLiteral("PM95AA12")
                           << QPointF(138.0125, 35.010416666666664);
    QTest::newRow("grid-before-dxcc") << QStringLiteral("AA00") << QPointF(-179, -89.5);
}

void TargetBearingTests::resolvesGrids()
{
    QFETCH(QString, input);
    QFETCH(QPointF, coordinates);
    const auto target = TargetBearing::resolve(input);
    QVERIFY(target);
    QCOMPARE(target->type, TargetBearing::TargetType::Grid);
    QCOMPARE(target->input, input.trimmed().toUpper());
    QVERIFY(qAbs(target->coordinates.x() - coordinates.x()) < 1e-10);
    QVERIFY(qAbs(target->coordinates.y() - coordinates.y()) < 1e-10);
    QVERIFY(target->entityName.isEmpty());
}

void TargetBearingTests::resolvesDxcc_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("prefix");
    QTest::addColumn<QPointF>("coordinates");
    const auto row = [](const char *name, const char *input, const char *prefix, double lon, double lat) {
        QTest::newRow(name) << QString::fromLatin1(input) << QString::fromLatin1(prefix) << QPointF(lon, lat);
    };
    row("china-call", " bg5geb ", "BY", 102, 36);
    row("china-prefix", "BY", "BY", 102, 36);
    row("district-prefix", "BG5", "BY", 102, 36);
    row("regional-alias-uses-entity-center", "BG9ABC", "BY", 102, 36);
    row("japan", "JA1ABC", "JA", 138.38, 36.4);
    row("japan-prefix", "JA", "JA", 138.38, 36.4);
    row("us-short-prefix", "W", "K", -91.87, 37.6);
    row("us-call", "K1ABC", "K", -91.87, 37.6);
    row("longest-prefix", "KH6ABC", "KH6", -157.48, 21.12);
    row("full-call-exception", "K2GT", "KH6", -157.48, 21.12);
    row("itu-exception", "4U1ITU", "4U1I", 6.05, 46.17);
    row("un-exception", "4U1UN", "4U1U", -73.97, 40.75);
    row("ignore-wae-exception", "4U1VIC", "OE", 13.33, 47.33);
    row("ignore-wae-prefix", "IT9ABC", "I", 12.58, 42.82);
    row("wae-district-prefix", "IT9", "I", 12.58, 42.82);
    row("wae-turkey", "TA1ABC", "TA", 35.65, 39.18);
    row("portable-prefix", "EA8/K1ABC", "EA8", -15.85, 28.32);
    row("portable-suffix", "K1ABC/EA8", "EA8", -15.85, 28.32);
    row("portable-three-parts", "EA8/K1ABC/P", "EA8", -15.85, 28.32);
    row("portable-short-prefix", "F/K1ABC", "F", 2, 46);
    row("portable-short-suffix", "K1ABC/F", "F", 2, 46);
    row("portable-indicator", "JA1ABC/P", "JA", 138.38, 36.4);
    row("mobile-indicator", "JA1ABC/M", "JA", 138.38, 36.4);
    row("qrp-indicator", "JA1ABC/QRP", "JA", 138.38, 36.4);
    row("exception-with-indicator", "K2GT/P", "KH6", -157.48, 21.12);
    row("district-changes-dxcc", "EA3ABC/8", "EA8", -15.85, 28.32);
    row("portable-exact-exception", "EA3JAQ/9", "EA9", -5.27, 35.90);
    row("exact-before-longest-prefix", "JD1YBJ", "JD/M", 153.97, 24.28);
    row("southern-hemisphere", "ZL1ABC", "ZL", 174.47, -39.03);
    row("synthetic-primary-prefix", "3D2/c", "3D2/C", 175, -22);
}

void TargetBearingTests::resolvesDxcc()
{
    QFETCH(QString, input);
    QFETCH(QString, prefix);
    QFETCH(QPointF, coordinates);
    const auto target = TargetBearing::resolve(input);
    QVERIFY2(target.has_value(), qPrintable(input));
    QCOMPARE(target->type, TargetBearing::TargetType::Dxcc);
    QCOMPARE(target->primaryPrefix, prefix);
    QCOMPARE(target->coordinates, coordinates);
    QVERIFY(!target->entityName.isEmpty());
}

void TargetBearingTests::rejectsUnknownTargets()
{
    const QStringList invalid = {
        {}, "Q1ABC", "garbage", "GARBAGE1TEST", "JA 1ABC", "JA1ABC!",
        "/JA1ABC", "JA1ABC/", "JA1ABC//P", "JA1ABC/MM", "JA1ABC/AM",
        "EA8/K1ABC/MM", "K1ABC/Q0", "K1ABC/JA1ABC", "EA8/K1ABC/F",
        "VERSION", "VER20261114", QString::fromUtf8("Gß1ABC"), QString(65, QLatin1Char('A'))
    };
    for (const QString &input : invalid)
        QVERIFY2(!TargetBearing::resolve(input), qPrintable(input));
}

void TargetBearingTests::bearings_data()
{
    QTest::addColumn<QPointF>("station");
    QTest::addColumn<QPointF>("target");
    QTest::addColumn<double>("expected");
    QTest::newRow("north") << QPointF(0, 0) << QPointF(0, 40) << 0.0;
    QTest::newRow("east") << QPointF(0, 0) << QPointF(40, 0) << 90.0;
    QTest::newRow("south") << QPointF(0, 0) << QPointF(0, -40) << 180.0;
    QTest::newRow("west") << QPointF(0, 0) << QPointF(-40, 0) << 270.0;
    QTest::newRow("date-line-east") << QPointF(179, 0) << QPointF(-179, 0) << 90.0;
    QTest::newRow("date-line-west") << QPointF(-179, 0) << QPointF(179, 0) << 270.0;
    // NOTE: 独立参考值由 GeographicLib 在半径 6371000 m、扁率为 0 的球面上求得。
    QTest::newRow("jfk-lhr") << QPointF(-73.7781, 40.6413) << QPointF(-0.4543, 51.4700)
                             << 51.35252086642547;
    QTest::newRow("target-pole") << QPointF(50, 20) << QPointF(-70, 90) << 0.0;
}

void TargetBearingTests::bearings()
{
    QFETCH(QPointF, station);
    QFETCH(QPointF, target);
    QFETCH(double, expected);
    const auto bearing = TargetBearing::initialBearing(station, target);
    QVERIFY(bearing);
    QVERIFY(*bearing >= 0 && *bearing < 360);
    QVERIFY(qAbs(*bearing - expected) < 1e-8);
}

void TargetBearingTests::rejectsUndefinedBearings()
{
    QVERIFY(!TargetBearing::initialBearing(QPointF(20, 30), QPointF(20, 30)));
    QVERIFY(!TargetBearing::initialBearing(QPointF(20, 30), QPointF(-160, -30)));
    QVERIFY(!TargetBearing::initialBearing(QPointF(180, 0), QPointF(-180, 0)));
    QVERIFY(!TargetBearing::initialBearing(QPointF(0, 90), QPointF(0, 0)));
    for (const QPointF point : {QPointF(181, 0), QPointF(0, -91),
                               QPointF(std::numeric_limits<double>::quiet_NaN(), 0),
                               QPointF(0, std::numeric_limits<double>::infinity())}) {
        QVERIFY(!TargetBearing::initialBearing(point, QPointF(0, 0)));
        QVERIFY(!TargetBearing::initialBearing(QPointF(0, 0), point));
    }
}

void TargetBearingTests::calculatesFromStationGrid()
{
    const auto grid = TargetBearing::calculate(QStringLiteral("PM95AA"), QStringLiteral("FN31"));
    QCOMPARE(grid.error, TargetBearing::Error::None);
    QVERIFY(grid.solution);
    QVERIFY(qAbs(grid.solution->degrees - 22.976117769332095) < 1e-8);
    QCOMPARE(grid.solution->roundedDegrees(), 23);
    const auto call = TargetBearing::calculate(QStringLiteral(" pm95aa "), QStringLiteral("ja1abc"));
    QVERIFY(call.solution);
    QVERIFY(qAbs(call.solution->degrees - 11.166962485192187) < 1e-8);
    QCOMPARE(call.solution->roundedDegrees(), 11);
    const TargetBearing::Solution nearNorth{call.solution->target, 359.8};
    QCOMPARE(nearNorth.roundedDegrees(), 0);
}

void TargetBearingTests::reportsErrors()
{
    for (const QString station : {QString(), QStringLiteral("ZZ99")}) {
        const auto result = TargetBearing::calculate(station, QStringLiteral("JA"));
        QCOMPARE(result.error, TargetBearing::Error::InvalidStationGrid);
        QVERIFY(!result.solution);
    }
    const auto unknown = TargetBearing::calculate(QStringLiteral("PM95AA"), QStringLiteral("Q1ABC"));
    QCOMPARE(unknown.error, TargetBearing::Error::UnknownTarget);
    QVERIFY(!unknown.solution);
    for (const QString target : {QStringLiteral("AA00"), QStringLiteral("JR09")}) {
        const auto result = TargetBearing::calculate(QStringLiteral("AA00"), target);
        QCOMPARE(result.error, TargetBearing::Error::UndefinedBearing);
        QVERIFY(!result.solution);
    }
}

QTEST_GUILESS_MAIN(TargetBearingTests)
#include "TargetBearingTests.moc"
