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

#include "AzimuthScale.h"
#include "targeting/TargetBearing.h"
#include "wsjtx/WsjtXController.h"
#include "wsjtx/WsjtXProtocol.h"

#include <QDataStream>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QtTest>

namespace {

QByteArray statusPacket(const QByteArray &ownCall, const QByteArray &ownGrid,
                        const QByteArray &dxCall = {}, const QByteArray &dxGrid = {},
                        quint32 schema = 3, bool transmitting = false)
{
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(schema == 2 ? QDataStream::Qt_5_2 : QDataStream::Qt_5_4);
    stream << quint32(0xadbccbda) << schema << quint32(1) << QByteArray("test-instance")
           << quint64(14074000) << QByteArray("FT8") << dxCall << QByteArray("-10")
           << QByteArray("FT8") << true << transmitting << false << quint32(0) << quint32(0)
           << ownCall << ownGrid << dxGrid;
    return packet;
}

void send(QUdpSocket &socket, const QByteArray &packet)
{
    QCOMPARE(socket.writeDatagram(packet, QHostAddress::LocalHost, WsjtX::ControlPort), packet.size());
}

QString expectedCommand(const QString &target, int current = 350, int offset = 0,
                        bool shortestPath = true, bool longPath = false)
{
    const auto result = TargetBearing::calculate(QStringLiteral("PM95AA"), target);
    Q_ASSERT(result.solution);
    const int bearing = longPath
        ? AzimuthScale::normalized(result.solution->roundedDegrees() + 180)
        : result.solution->roundedDegrees();
    const AzimuthScale scale(offset);
    const auto direct = scale.mechanical(bearing);
    const int mechanical = !shortestPath && direct
        ? *direct : scale.mechanicalBearing(bearing, shortestPath ? current : 0);
    return QStringLiteral("M%1").arg(mechanical, 3, 10, QLatin1Char('0'));
}

} // namespace

class WsjtXTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesSharedStatusSchemas();
    void rejectsInvalidDatagrams();
    void tracksDxTargetChangesAndAppliesSettings();
    void followsFirstAndChangedTargetsButIgnoresOwn();
    void rejectsSelectionOutsideTrackingAndFreshPosition();
    void reportsPortConflictsAndReleasesPort();
};

void WsjtXTests::parsesSharedStatusSchemas()
{
    for (quint32 schema : {2u, 3u}) {
        const auto status = WsjtX::parseStatusDatagram(
            statusPacket("ja1abc", "pm95aa", "k1abc", "fn42", schema));
        QVERIFY(status);
        QCOMPARE(status->ownCallsign, QStringLiteral("JA1ABC"));
        QCOMPARE(status->ownGrid, QStringLiteral("PM95AA"));
        QCOMPARE(status->dxCallsign, QStringLiteral("K1ABC"));
        QCOMPARE(status->dxGrid, QStringLiteral("FN42"));
    }

    const auto noTarget = WsjtX::parseStatusDatagram(statusPacket("ja1abc", "pm95aa"));
    QVERIFY(noTarget);
    QVERIFY(noTarget->dxCallsign.isEmpty());
    QVERIFY(noTarget->dxGrid.isEmpty());
}

void WsjtXTests::rejectsInvalidDatagrams()
{
    const QByteArray valid = statusPacket("JA1ABC", "PM95AA", "K1ABC", "FN42");
    QVERIFY(!WsjtX::parseStatusDatagram({}));
    QVERIFY(!WsjtX::parseStatusDatagram(valid.left(12)));
    QVERIFY(!WsjtX::parseStatusDatagram(valid + QByteArray(8193, 'x')));
    QVERIFY(!WsjtX::parseStatusDatagram(statusPacket("JA1ABC", "PM95AA", "K1ABC", "FN42", 4)));

    QByteArray invalidLength = valid.left(12) + QByteArray::fromHex("ffffffff");
    QVERIFY(!WsjtX::parseStatusDatagram(invalidLength));
}

void WsjtXTests::tracksDxTargetChangesAndAppliesSettings()
{
    WsjtXController controller;
    controller.setStationGrid(QStringLiteral("PM95AA"));
    controller.setControlAvailable(true);
    controller.setTrackingEnabled(true);
    controller.setPosition(350);
    QVERIFY(controller.setEnabled(true));
    QSignalSpy commands(&controller, &WsjtXController::commandRequested);
    QUdpSocket sender;

    send(sender, statusPacket("JA1ABC", "PM95AA"));
    send(sender, statusPacket("JA1ABC", "PM95AA", {}, {}, 3, true));
    QTest::qWait(40);
    QCOMPARE(commands.size(), 0);

    send(sender, statusPacket("JA1ABC", "PM95AA", "K1ABC", "FN42"));
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), expectedCommand(QStringLiteral("FN42")));

    send(sender, statusPacket("JA1ABC", "PM95AA", "K1ABC", "FN42", 3, true));
    QTest::qWait(40);
    QCOMPARE(commands.size(), 0);

    controller.setAzimuthOffset(15);
    controller.setShortestPathEnabled(false);
    controller.setLongPathEnabled(true);
    send(sender, statusPacket("JA1ABC", "PM95AA", "K1ABC"));
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(),
             expectedCommand(QStringLiteral("K1ABC"), 350, 15, false, true));

    send(sender, statusPacket("JA1ABC", "PM95AA", "W9XYZ", "EN37"));
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(),
             expectedCommand(QStringLiteral("EN37"), 350, 15, false, true));
}

void WsjtXTests::followsFirstAndChangedTargetsButIgnoresOwn()
{
    WsjtXController controller;
    controller.setControlAvailable(true);
    controller.setTrackingEnabled(true);
    controller.setPosition(100);
    QVERIFY(controller.setEnabled(true));
    QSignalSpy commands(&controller, &WsjtXController::commandRequested);
    QUdpSocket sender;

    send(sender, statusPacket("W9XYZ", "PM95AA", "K1ABC", "FN42"));
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), expectedCommand(QStringLiteral("FN42"), 100));

    send(sender, statusPacket("W9XYZ", "PM95AA", "W9XYZ", ""));
    QTest::qWait(40);
    QCOMPARE(commands.size(), 0);

    send(sender, statusPacket("W9XYZ", "PM95AA", "K1ABC", "FN42"));
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), expectedCommand(QStringLiteral("FN42"), 100));

    send(sender, statusPacket("W9XYZ", "PM95AA"));
    QTest::qWait(40);
    send(sender, statusPacket("W9XYZ", "PM95AA", "K1ABC", "FN42"));
    QTRY_COMPARE(commands.size(), 1);
}

void WsjtXTests::rejectsSelectionOutsideTrackingAndFreshPosition()
{
    WsjtXController controller;
    controller.setStationGrid(QStringLiteral("PM95AA"));
    controller.setControlAvailable(true);
    controller.setPosition(0);
    QVERIFY(controller.setEnabled(true));
    QSignalSpy commands(&controller, &WsjtXController::commandRequested);
    QSignalSpy errors(&controller, &WsjtXController::errorOccurred);
    QUdpSocket sender;

    send(sender, statusPacket("JA1ABC", "PM95AA"));
    send(sender, statusPacket("JA1ABC", "PM95AA", "K1ABC", "FN42"));
    QTest::qWait(40);
    QCOMPARE(commands.size(), 0);

    controller.setTrackingEnabled(true);
    send(sender, statusPacket("JA1ABC", "PM95AA", "K1ABC", "FN42"));
    send(sender, statusPacket("JA1ABC", "PM95AA", "W9XYZ", "EN37"));
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), expectedCommand(QStringLiteral("EN37"), 0));

    controller.setControlAvailable(false);
    controller.setControlAvailable(true);
    send(sender, statusPacket("JA1ABC", "PM95AA", "K1ABC", "FN42"));
    QTRY_COMPARE(errors.size(), 1);
    QCOMPARE(commands.size(), 0);

    controller.setPosition(0);
    send(sender, statusPacket("JA1ABC", "PM95AA", "W9XYZ", "EN37"));
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().at(0).toString(), expectedCommand(QStringLiteral("EN37"), 0));
}

void WsjtXTests::reportsPortConflictsAndReleasesPort()
{
    QUdpSocket owner;
    QVERIFY(owner.bind(QHostAddress::LocalHost, WsjtX::ControlPort,
                       QUdpSocket::DontShareAddress));
    WsjtXController controller;
    QSignalSpy errors(&controller, &WsjtXController::errorOccurred);
    QVERIFY(!controller.setEnabled(true));
    QVERIFY(!controller.isEnabled());
    QCOMPARE(errors.size(), 1);
    owner.close();
    QVERIFY(controller.setEnabled(true));
    QVERIFY(controller.setEnabled(false));
    QUdpSocket replacement;
    QVERIFY(replacement.bind(QHostAddress::LocalHost, WsjtX::ControlPort,
                             QUdpSocket::DontShareAddress));
}

QTEST_GUILESS_MAIN(WsjtXTests)
#include "WsjtXTests.moc"
