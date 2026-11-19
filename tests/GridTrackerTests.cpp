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

#include "gridtracker/GridTrackerController.h"
#include "gridtracker/GridTrackerProtocol.h"

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QtTest>

#include <limits>

namespace {

const QByteArray Turn = "<PST><AZIMUTH>10</AZIMUTH></PST>";
const QByteArray Poll = "<PST>AZ?</PST><PST>EL?</PST>";

void send(QUdpSocket &socket, const QByteArray &packet)
{
    QCOMPARE(socket.writeDatagram(packet, QHostAddress::LocalHost, GridTracker::ControlPort), packet.size());
}

} // namespace

class GridTrackerTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesGridTrackerPackets();
    void rejectsMalformedDatagrams_data();
    void rejectsMalformedDatagrams();
    void convertsHeadingsAndPreservesOtherAxis();
    void reportsOnlyFreshMeasuredAxes();
    void dropsUnavailableAndQueuedCommands();
    void reportsPortConflictsAndReleasesPort();
};

void GridTrackerTests::parsesGridTrackerPackets()
{
    auto requests = GridTracker::parseDatagram(Turn);
    QVERIFY(requests && requests->size() == 1);
    QCOMPARE(requests->front().kind, GridTracker::Request::Kind::Turn);
    QCOMPARE(requests->front().azimuth.value(), 10);
    QVERIFY(!requests->front().elevation);

    requests = GridTracker::parseDatagram("<PST><AZIMUTH>359.6</AZIMUTH><ELEVATION>45.4</ELEVATION></PST>");
    QVERIFY(requests);
    QCOMPARE(requests->front().azimuth.value(), 360);
    QCOMPARE(requests->front().elevation.value(), 45);
    requests = GridTracker::parseDatagram("<PST><ELEVATION>0</ELEVATION></PST>");
    QVERIFY(requests);
    QVERIFY(!requests->front().azimuth);
    QCOMPARE(requests->front().elevation.value(), 0);
    requests = GridTracker::parseDatagram(" \n" + Poll + "\r\n");
    QVERIFY(requests && requests->size() == 2);
    QCOMPARE(requests->at(0).kind, GridTracker::Request::Kind::QueryAzimuth);
    QCOMPARE(requests->at(1).kind, GridTracker::Request::Kind::QueryElevation);
    QVERIFY(GridTracker::parseDatagram("<PST><AZIMUTH>0</AZIMUTH><ELEVATION>180</ELEVATION></PST>"));
}

void GridTrackerTests::rejectsMalformedDatagrams_data()
{
    QTest::addColumn<QByteArray>("packet");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("empty-root") << QByteArray("<PST/>");
    QTest::newRow("too-large") << Turn + QByteArray(8193, ' ');
    QTest::newRow("truncated") << QByteArray("<PST><AZIMUTH>10</AZIMUTH>");
    QTest::newRow("wrong-protocol") << QByteArray("<N1MMRotor><goazi>10</goazi></N1MMRotor>");
    QTest::newRow("negative") << QByteArray("<PST><AZIMUTH>-1</AZIMUTH></PST>");
    QTest::newRow("azimuth-range") << QByteArray("<PST><AZIMUTH>360.1</AZIMUTH></PST>");
    QTest::newRow("elevation-range") << QByteArray("<PST><ELEVATION>180.1</ELEVATION></PST>");
    QTest::newRow("nan") << QByteArray("<PST><AZIMUTH>nan</AZIMUTH></PST>");
    QTest::newRow("infinity") << QByteArray("<PST><AZIMUTH>inf</AZIMUTH></PST>");
    QTest::newRow("overflow") << QByteArray("<PST><AZIMUTH>1e999</AZIMUTH></PST>");
    QTest::newRow("duplicate") << QByteArray("<PST><AZIMUTH>10</AZIMUTH><AZIMUTH>20</AZIMUTH></PST>");
    QTest::newRow("nested") << QByteArray("<PST><AZIMUTH><X>10</X></AZIMUTH></PST>");
    QTest::newRow("mixed-query-turn") << QByteArray("<PST>AZ?<AZIMUTH>10</AZIMUTH></PST>");
    QTest::newRow("unsupported") << QByteArray("<PST><STOP>1</STOP></PST>");
    QTest::newRow("attribute") << QByteArray("<PST rotor='2'><AZIMUTH>10</AZIMUTH></PST>");
    QTest::newRow("namespace") << QByteArray("<PST xmlns='other'><AZIMUTH>10</AZIMUTH></PST>");
    QTest::newRow("field-namespace") << QByteArray("<PST><x:AZIMUTH xmlns:x='other'>10</x:AZIMUTH></PST>");
    QTest::newRow("dtd") << QByteArray("<!DOCTYPE PST [<!ENTITY a '10'>]><PST><AZIMUTH>&a;</AZIMUTH></PST>");
    QTest::newRow("trailing-garbage") << Turn + "junk";
    QTest::newRow("bad-second-request") << Turn + "<PST><AZIMUTH>bad</AZIMUTH></PST>";
    QTest::newRow("wrapper-escape") << QByteArray("</datagram>") + Turn + "<datagram>";
}

void GridTrackerTests::rejectsMalformedDatagrams()
{
    QFETCH(QByteArray, packet);
    QVERIFY(!GridTracker::parseDatagram(packet));
}

void GridTrackerTests::convertsHeadingsAndPreservesOtherAxis()
{
    GridTrackerController controller;
    controller.setTrackingEnabled(true);
    controller.setControlAvailable(true);
    controller.setAzimuthPosition(350);
    QVERIFY(controller.setEnabled(true));
    QSignalSpy commands(&controller, &GridTrackerController::commandRequested);
    QUdpSocket sender;
    send(sender, Turn);
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), QStringLiteral("M370"));

    controller.setShortestPathEnabled(false);
    send(sender, Turn);
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), QStringLiteral("M010"));
    controller.setShortestPathEnabled(true);
    controller.setAzimuthOffset(30);
    send(sender, "<PST><AZIMUTH>10</AZIMUTH><ELEVATION>45</ELEVATION></PST>");
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), QStringLiteral("W340 045"));
    send(sender, "<PST><ELEVATION>0</ELEVATION></PST>");
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), QStringLiteral("W350 000"));

    controller.setAzimuthOffset(std::numeric_limits<int>::min());
    send(sender, Turn);
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), QStringLiteral("M138"));
    send(sender, Turn + "<PST>invalid</PST>");
    QTest::qWait(30);
    QVERIFY(commands.isEmpty());
}

void GridTrackerTests::reportsOnlyFreshMeasuredAxes()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, GridTracker::ReportingPort, QUdpSocket::DontShareAddress));
    GridTrackerController controller;
    controller.setTrackingEnabled(true);
    QVERIFY(controller.setEnabled(true));
    controller.setControlAvailable(true);
    QSignalSpy errors(&controller, &GridTrackerController::errorOccurred);
    QSignalSpy commands(&controller, &GridTrackerController::commandRequested);
    QUdpSocket sender;
    send(sender, Poll);
    send(sender, Turn);
    QTRY_COMPARE(errors.size(), 1);
    QVERIFY(!receiver.hasPendingDatagrams());
    QVERIFY(commands.isEmpty());
    controller.setAzimuthOffset(30);
    controller.setAzimuthPosition(350);
    send(sender, Poll);
    QTRY_VERIFY(receiver.hasPendingDatagrams());
    QCOMPARE(receiver.receiveDatagram().data(), QByteArray("AZ:20"));
    QVERIFY(!receiver.hasPendingDatagrams());
    controller.setElevationPosition(45);
    send(sender, Turn);
    QTRY_COMPARE(commands.size(), 1);
    QVERIFY(!receiver.hasPendingDatagrams());
    send(sender, Poll);
    QTRY_VERIFY(receiver.hasPendingDatagrams());
    QCOMPARE(receiver.receiveDatagram().data(), QByteArray("AZ:20"));
    QTRY_VERIFY(receiver.hasPendingDatagrams());
    QCOMPARE(receiver.receiveDatagram().data(), QByteArray("EL:45"));

    QTest::qWait(3050);
    controller.setElevationPosition(30);
    send(sender, Poll);
    send(sender, Turn);
    QTRY_COMPARE(errors.size(), 2);
    QTRY_VERIFY(receiver.hasPendingDatagrams());
    QCOMPARE(receiver.receiveDatagram().data(), QByteArray("EL:30"));
    QVERIFY(!receiver.hasPendingDatagrams());
    QCOMPARE(commands.size(), 1);
    controller.setAzimuthPosition(451);
    controller.setElevationPosition(181);
    send(sender, Poll);
    QTest::qWait(30);
    QVERIFY(!receiver.hasPendingDatagrams());
}

void GridTrackerTests::dropsUnavailableAndQueuedCommands()
{
    GridTrackerController controller;
    controller.setTrackingEnabled(true);
    QVERIFY(controller.setEnabled(true));
    QUdpSocket sender;
    QSignalSpy commands(&controller, &GridTrackerController::commandRequested);
    QSignalSpy errors(&controller, &GridTrackerController::errorOccurred);
    send(sender, Turn);
    controller.setControlAvailable(true);
    controller.setAzimuthPosition(350);
    QTest::qWait(30);
    QVERIFY(commands.isEmpty());
    send(sender, Turn);
    controller.setControlAvailable(false);
    controller.setAzimuthPosition(350);
    send(sender, Turn);
    controller.setControlAvailable(true);
    send(sender, Turn);
    QTRY_COMPARE(errors.size(), 1);
    QVERIFY(commands.isEmpty());
    controller.setAzimuthPosition(350);
    send(sender, Turn);
    QVERIFY(controller.setEnabled(false));
    QVERIFY(controller.setEnabled(true));
    QTest::qWait(30);
    QVERIFY(commands.isEmpty());
    send(sender, Turn);
    QTRY_COMPARE(commands.size(), 1);
}

void GridTrackerTests::reportsPortConflictsAndReleasesPort()
{
    GridTrackerController first;
    GridTrackerController second;
    QVERIFY(first.setEnabled(true));
    QVERIFY(first.setEnabled(true));
    QSignalSpy errors(&second, &GridTrackerController::errorOccurred);
    QVERIFY(!second.setEnabled(true));
    QVERIFY(!second.isEnabled());
    QVERIFY(!second.errorString().isEmpty());
    QCOMPARE(errors.size(), 1);
    QVERIFY(first.setEnabled(false));
    QVERIFY(second.setEnabled(true));
    QVERIFY(second.errorString().isEmpty());
}

QTEST_GUILESS_MAIN(GridTrackerTests)
#include "GridTrackerTests.moc"
