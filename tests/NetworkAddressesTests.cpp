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

#include "NetworkAddresses.h"

#include <QtTest>

class NetworkAddressesTests final : public QObject
{
    Q_OBJECT

private slots:
    void readsRuntimeAddresses();
    void ignoresInactiveInterfaces_data();
    void ignoresInactiveInterfaces();
    void rejectsInvalidAddresses_data();
    void rejectsInvalidAddresses();
    void ignoresSavedConfiguration();
};

void NetworkAddressesTests::readsRuntimeAddresses()
{
    const QJsonObject network{
        {"active", QJsonObject{{"eth", QJsonObject{{"address", "192.0.2.99"}}}}},
        {"runtime", QJsonObject{
            {"eth", QJsonObject{{"link_up", true}, {"ipv4_valid", true},
                                {"address", "192.0.2.10"}}},
            {"wifi", QJsonObject{{"connected", true}, {"ipv4_valid", true},
                                 {"address", "198.51.100.20"}}}}}};
    const QJsonObject provisioning{{"state", "active"},
                                   {"ap", QJsonObject{{"address", "192.168.4.1"}}}};
    const auto addresses = NetworkAddresses::fromResponses(network, provisioning);
    QCOMPARE(addresses.ethernet, QStringLiteral("192.0.2.10"));
    QCOMPARE(addresses.station, QStringLiteral("198.51.100.20"));
    QCOMPARE(addresses.accessPoint, QStringLiteral("192.168.4.1"));
}

void NetworkAddressesTests::ignoresInactiveInterfaces_data()
{
    QTest::addColumn<bool>("connected");
    QTest::addColumn<bool>("ipv4Valid");
    QTest::addColumn<QString>("apState");

    QTest::newRow("disconnected-with-stale-lease") << false << true << QStringLiteral("off");
    QTest::newRow("awaiting-ip") << true << false << QStringLiteral("expired");
    QTest::newRow("disconnected") << false << false << QString();
}

void NetworkAddressesTests::ignoresInactiveInterfaces()
{
    QFETCH(bool, connected);
    QFETCH(bool, ipv4Valid);
    QFETCH(QString, apState);

    const QJsonObject network{{"runtime", QJsonObject{
        {"eth", QJsonObject{{"link_up", connected}, {"ipv4_valid", ipv4Valid},
                            {"address", "192.0.2.10"}}},
        {"wifi", QJsonObject{{"connected", connected}, {"ipv4_valid", ipv4Valid},
                             {"address", "198.51.100.20"}}}}}};
    const QJsonObject provisioning{{"state", apState},
                                   {"ap", QJsonObject{{"address", "192.168.4.1"}}}};
    const auto addresses = NetworkAddresses::fromResponses(network, provisioning);
    QVERIFY(addresses.ethernet.isEmpty());
    QVERIFY(addresses.station.isEmpty());
    QVERIFY(addresses.accessPoint.isEmpty());
}

void NetworkAddressesTests::rejectsInvalidAddresses_data()
{
    QTest::addColumn<QJsonValue>("address");
    QTest::newRow("zero") << QJsonValue(QStringLiteral("0.0.0.0"));
    QTest::newRow("empty") << QJsonValue(QString());
    QTest::newRow("malformed") << QJsonValue(QStringLiteral("192.0.2.999"));
    QTest::newRow("ipv6") << QJsonValue(QStringLiteral("fe80::1"));
    QTest::newRow("null") << QJsonValue(QJsonValue::Null);
    QTest::newRow("wrong-type") << QJsonValue(123);
}

void NetworkAddressesTests::rejectsInvalidAddresses()
{
    QFETCH(QJsonValue, address);

    const QJsonObject network{{"runtime", QJsonObject{
        {"eth", QJsonObject{{"link_up", true}, {"ipv4_valid", true}, {"address", address}}},
        {"wifi", QJsonObject{{"connected", true}, {"ipv4_valid", true},
                             {"address", address}}}}}};
    const QJsonObject provisioning{{"state", "active"},
                                   {"ap", QJsonObject{{"address", address}}}};
    const auto addresses = NetworkAddresses::fromResponses(network, provisioning);
    QVERIFY(addresses.ethernet.isEmpty());
    QVERIFY(addresses.station.isEmpty());
    QVERIFY(addresses.accessPoint.isEmpty());
}

void NetworkAddressesTests::ignoresSavedConfiguration()
{
    const QJsonObject configuration{
        {"eth", QJsonObject{{"address", "192.0.2.10"}}},
        {"wifi", QJsonObject{{"address", "198.51.100.20"}}}};
    const QJsonObject network{{"active", configuration}, {"candidate", configuration},
                              {"trial", QJsonObject{{"config", configuration}}},
                              {"runtime", QJsonValue::Null}};
    const auto addresses = NetworkAddresses::fromResponses(network, {});
    QVERIFY(addresses.ethernet.isEmpty());
    QVERIFY(addresses.station.isEmpty());
    QVERIFY(addresses.accessPoint.isEmpty());
}

QTEST_GUILESS_MAIN(NetworkAddressesTests)
#include "NetworkAddressesTests.moc"
