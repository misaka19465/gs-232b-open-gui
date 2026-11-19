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

#include "n1mm/N1mmController.h"
#include "n1mm/N1mmProtocol.h"
#include "AzimuthScale.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QtTest>

#include <limits>
#include <cmath>

namespace {

QByteArray turn(const QByteArray &name = "GS232B", const QByteArray &heading = "10")
{
    return "<N1MMRotor><rotor>" + name + "</rotor><goazi>" + heading +
           "</goazi><offset>0.0</offset><bidirectional>0</bidirectional><freqband>14</freqband></N1MMRotor>";
}

QByteArray stop(const QByteArray &name = "GS232B")
{
    return "<N1MMRotor><stop><rotor>" + name + "</rotor><freqband>21.0</freqband></stop></N1MMRotor>";
}

QByteArray receive(QUdpSocket &socket)
{
    if (!QTest::qWaitFor([&] { return socket.hasPendingDatagrams(); }, 1000))
        return {};
    return socket.receiveDatagram().data();
}

} // namespace

class N1mmTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesOfficialPackets();
    void rejectsInvalidPackets_data();
    void rejectsInvalidPackets();
    void validatesNames_data();
    void validatesNames();
    void resolvesTargets_data();
    void resolvesTargets();
    void resolvesExtremeLocalOffsets();
    void routesNamedCommandsAndStops();
    void reportsMeasuredPositionAndNameChanges();
    void dropsUnavailableAndStaleCommands();
    void disablingDropsQueuedCommands();
    void reportsPortConflicts();
};

void N1mmTests::parsesOfficialPackets()
{
    const auto command = N1mm::parseCommand(turn("Tower A,GS232B", "55.0"));
    QVERIFY(command);
    QCOMPARE(command->kind, N1mm::Command::Kind::Turn);
    QCOMPARE(command->rotors, QStringList({"Tower A", "GS232B"}));
    QCOMPARE(command->azimuth, 55.0);
    QCOMPARE(command->offset, 0.0);
    QVERIFY(!command->bidirectional);

    const auto stopped = N1mm::parseCommand(stop("YaesuCom9"));
    QVERIFY(stopped);
    QCOMPARE(stopped->kind, N1mm::Command::Kind::Stop);
    QCOMPARE(stopped->rotors, QStringList({"YaesuCom9"}));
    QVERIFY(N1mm::parseCommand("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" + turn()));
    const auto escaped = N1mm::parseCommand(turn("Tower &amp; Beam"));
    QVERIFY(escaped);
    QCOMPARE(escaped->rotors.first(), QStringLiteral("Tower & Beam"));
    QVERIFY(N1mm::parseCommand("<N1MMRotor><goazi>360</goazi><rotor>3</rotor></N1MMRotor>"));
}

void N1mmTests::rejectsInvalidPackets_data()
{
    QTest::addColumn<QByteArray>("packet");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("oversize") << QByteArray(8193, 'x');
    QTest::newRow("truncated") << turn().chopped(1);
    QTest::newRow("not-rotor") << QByteArray("<RadioInfo><goazi>10</goazi></RadioInfo>");
    QTest::newRow("missing-heading") << QByteArray("<N1MMRotor><rotor>GS232B</rotor></N1MMRotor>");
    QTest::newRow("missing-name") << QByteArray("<N1MMRotor><goazi>10</goazi></N1MMRotor>");
    QTest::newRow("duplicate-name") << QByteArray("<N1MMRotor><rotor>A</rotor><rotor>GS232B</rotor><goazi>10</goazi></N1MMRotor>");
    QTest::newRow("empty-name") << turn("");
    QTest::newRow("empty-stack-name") << turn("GS232B,");
    QTest::newRow("nan") << turn("GS232B", "nan");
    QTest::newRow("infinity") << turn("GS232B", "inf");
    QTest::newRow("too-high") << turn("GS232B", "450.1");
    QTest::newRow("negative") << turn("GS232B", "-1");
    QTest::newRow("decimal-comma") << turn("GS232B", "10,5");
    QTest::newRow("invalid-offset") << turn().replace("<offset>0.0", "<offset>nan");
    QTest::newRow("invalid-bidirectionality") << turn().replace("<bidirectional>0", "<bidirectional>2");
    QTest::newRow("mixed-stop-and-turn") << stop().replace("</N1MMRotor>", "<goazi>100</goazi></N1MMRotor>");
    QTest::newRow("trailing-root") << turn() + turn();
    QTest::newRow("dtd") << QByteArray("<!DOCTYPE N1MMRotor [<!ENTITY n 'GS232B'>]>") + turn("&n;");
    QTest::newRow("nested-heading") << turn("GS232B", "<goazi>10</goazi>");
}

void N1mmTests::rejectsInvalidPackets()
{
    QFETCH(QByteArray, packet);
    QVERIFY(!N1mm::parseCommand(packet));
}

void N1mmTests::validatesNames_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("valid");
    QTest::newRow("default") << QStringLiteral("GS232B") << true;
    QTest::newRow("trimmed") << QStringLiteral(" Tower A ") << true;
    QTest::newRow("unicode") << QStringLiteral("主旋转器") << true;
    QTest::newRow("boundary") << QString(64, QLatin1Char('A')) << true;
    QTest::newRow("empty") << QString() << false;
    QTest::newRow("blank") << QStringLiteral("  ") << false;
    QTest::newRow("comma") << QStringLiteral("A,B") << false;
    QTest::newRow("at") << QStringLiteral("A@B") << false;
    QTest::newRow("newline") << QStringLiteral("A\nB") << false;
    QTest::newRow("tab") << QStringLiteral("A\tB") << false;
    QTest::newRow("null") << QStringLiteral("A") + QChar(0) << false;
    QTest::newRow("oversize") << QString(65, QLatin1Char('A')) << false;
}

void N1mmTests::validatesNames()
{
    QFETCH(QString, name);
    QFETCH(bool, valid);
    QCOMPARE(N1mm::isValidRotorName(name), valid);
}

void N1mmTests::resolvesTargets_data()
{
    QTest::addColumn<double>("heading");
    QTest::addColumn<double>("antennaOffset");
    QTest::addColumn<int>("localOffset");
    QTest::addColumn<int>("current");
    QTest::addColumn<bool>("shortest");
    QTest::addColumn<bool>("bidirectional");
    QTest::addColumn<int>("expected");
    QTest::newRow("overlap-shortest") << 10.0 << 0.0 << 0 << 350 << true << false << 370;
    QTest::newRow("overlap-continuous") << 10.0 << 0.0 << 0 << 350 << false << false << 10;
    QTest::newRow("positive-local-offset") << 10.0 << 0.0 << 30 << 350 << true << false << 340;
    QTest::newRow("negative-local-offset") << 330.0 << 0.0 << -30 << 350 << true << false << 360;
    QTest::newRow("antenna-offset") << 100.0 << 90.0 << 0 << 0 << true << false << 10;
    QTest::newRow("combined-offsets") << 100.0 << 90.0 << 30 << 350 << true << false << 340;
    QTest::newRow("negative-antenna-offset") << 100.0 << -90.0 << 0 << 0 << true << false << 190;
    QTest::newRow("fractional") << 1.5 << -0.5 << 0 << 0 << true << false << 2;
    QTest::newRow("bidirectional") << 190.0 << 0.0 << 0 << 10 << true << true << 10;
    QTest::newRow("bidirectional-overlap") << 250.0 << 0.0 << 0 << 440 << true << true << 430;
    QTest::newRow("full-travel") << 450.0 << 0.0 << 0 << 440 << false << false << 450;
    QTest::newRow("continuous-wrapped-compensation") << 10.0 << 0.0 << 30 << 350 << false << false << 340;
    QTest::newRow("tied-bidirectional") << 90.0 << 0.0 << 0 << 180 << true << true << 90;
}

void N1mmTests::resolvesTargets()
{
    QFETCH(double, heading);
    QFETCH(double, antennaOffset);
    QFETCH(int, localOffset);
    QFETCH(int, current);
    QFETCH(bool, shortest);
    QFETCH(bool, bidirectional);
    QFETCH(int, expected);
    N1mm::Command command;
    command.azimuth = heading;
    command.offset = antennaOffset;
    command.bidirectional = bidirectional;
    QCOMPARE(N1mm::mechanicalTarget(command, current, localOffset, shortest), expected);
}

void N1mmTests::resolvesExtremeLocalOffsets()
{
    for (int offset : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        for (double antennaOffset : {-std::numeric_limits<double>::max(), 0.0,
                                     std::numeric_limits<double>::max()}) {
            N1mm::Command command;
            command.azimuth = 450;
            command.offset = antennaOffset;
            for (int current = 0; current <= 450; ++current) {
                const int target = N1mm::mechanicalTarget(command, current, offset, true);
                QVERIFY(target >= 0 && target <= 450);
                QCOMPARE(AzimuthScale::normalized(AzimuthScale(offset).corrected(target)),
                         AzimuthScale::normalized(qRound64(450 - std::fmod(antennaOffset, 360.0))));
            }
        }
    }
}

void N1mmTests::routesNamedCommandsAndStops()
{
    N1mmController controller;
    controller.setTrackingEnabled(true);
    QVERIFY(controller.setEnabled(true));
    controller.setControlAvailable(true);
    controller.setPosition(350);
    QSignalSpy commands(&controller, &N1mmController::commandRequested);
    QUdpSocket sender;
    sender.writeDatagram(turn("Other"), QHostAddress::LocalHost, N1mm::ControlPort);
    sender.writeDatagram(turn("gs232b"), QHostAddress::LocalHost, N1mm::ControlPort);
    sender.writeDatagram(turn("Other, GS232B"), QHostAddress::LocalHost, N1mm::ControlPort);
    sender.writeDatagram(stop(), QHostAddress::LocalHost, N1mm::ControlPort);
    QTRY_COMPARE(commands.size(), 2);
    QCOMPARE(commands[0][0].toString(), QStringLiteral("M370"));
    QCOMPARE(commands[1][0].toString(), QStringLiteral("A"));
}

void N1mmTests::reportsMeasuredPositionAndNameChanges()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, N1mm::ReportingPort, QUdpSocket::DontShareAddress));
    N1mmController controller;
    controller.setControlAvailable(true);
    controller.setPosition(450);
    QVERIFY(!receiver.hasPendingDatagrams());
    controller.setAzimuthOffset(-30);
    QVERIFY(controller.setEnabled(true));
    QCOMPARE(receive(receiver), QByteArray("GS232B@600"));
    QVERIFY(controller.setRotorName(QStringLiteral(" Tower A ")));
    QCOMPARE(receive(receiver), QByteArray("Tower A@600"));
    QVERIFY(!controller.setRotorName(QStringLiteral("Bad@Name")));
    QCOMPARE(controller.rotorName(), QStringLiteral("Tower A"));
    controller.setPosition(10);
    QCOMPARE(receive(receiver), QByteArray("Tower A@3400"));
    controller.setAzimuthOffset(30);
    QCOMPARE(receive(receiver), QByteArray("Tower A@400"));
    controller.setControlAvailable(false);
    controller.setPosition(30);
    QVERIFY(!receiver.hasPendingDatagrams());
}

void N1mmTests::dropsUnavailableAndStaleCommands()
{
    N1mmController controller;
    controller.setTrackingEnabled(true);
    QVERIFY(controller.setEnabled(true));
    QSignalSpy commands(&controller, &N1mmController::commandRequested);
    QSignalSpy errors(&controller, &N1mmController::errorOccurred);
    QUdpSocket sender;
    sender.writeDatagram(turn(), QHostAddress::LocalHost, N1mm::ControlPort);
    controller.setControlAvailable(true);
    sender.writeDatagram(turn(), QHostAddress::LocalHost, N1mm::ControlPort);
    QTRY_COMPARE(errors.size(), 1);
    QVERIFY(commands.isEmpty());
    controller.setPosition(350);
    QTest::qWait(3100);
    sender.writeDatagram(turn(), QHostAddress::LocalHost, N1mm::ControlPort);
    sender.writeDatagram(stop(), QHostAddress::LocalHost, N1mm::ControlPort);
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(errors.size(), 2);
    QCOMPARE(commands[0][0].toString(), QStringLiteral("A"));
    controller.setPosition(350);
    controller.setControlAvailable(false);
    controller.setControlAvailable(true);
    sender.writeDatagram(turn(), QHostAddress::LocalHost, N1mm::ControlPort);
    QTRY_COMPARE(errors.size(), 3);
    QCOMPARE(commands.size(), 1);
}

void N1mmTests::disablingDropsQueuedCommands()
{
    N1mmController controller;
    controller.setTrackingEnabled(true);
    QVERIFY(controller.setEnabled(true));
    controller.setControlAvailable(true);
    controller.setPosition(350);
    QSignalSpy commands(&controller, &N1mmController::commandRequested);
    QUdpSocket sender;
    sender.writeDatagram(turn(), QHostAddress::LocalHost, N1mm::ControlPort);
    QVERIFY(controller.setEnabled(false));
    QVERIFY(!controller.isEnabled());
    QVERIFY(controller.setEnabled(true));
    sender.writeDatagram(stop(), QHostAddress::LocalHost, N1mm::ControlPort);
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands[0][0].toString(), QStringLiteral("A"));
}

void N1mmTests::reportsPortConflicts()
{
    N1mmController first;
    N1mmController second;
    QVERIFY(first.setEnabled(true));
    QVERIFY(!second.setEnabled(true));
    QVERIFY(!second.isEnabled());
    QVERIFY(!second.errorString().isEmpty());
    QVERIFY(first.setEnabled(false));
    QVERIFY(second.setEnabled(true));
}

QTEST_GUILESS_MAIN(N1mmTests)
#include "N1mmTests.moc"
