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

#include "FaultStatusMonitor.h"

#include <QSignalSpy>
#include <QtTest>

namespace {

QJsonObject statusResponse(const QJsonValue &code, int axes = 0)
{
    return {{"cmd", "MOTION STATUS"}, {"ok", true},
            {"data", QJsonObject{{"fault", QJsonObject{{"code", code}, {"axis_mask", axes}}}}}};
}

} // namespace

class FaultStatusMonitorTests final : public QObject
{
    Q_OBJECT

private slots:
    void decodesFirmwareFaults();
    void malformedStatusNeverMeansHealthy();
    void queriesWaitForResponse();
    void clearRequiresFaultAndVerification();
    void clearFailures_data();
    void clearFailures();
    void pauseAndDisconnectCancelOperations();
};

void FaultStatusMonitorTests::decodesFirmwareFaults()
{
    using Fault = FaultStatusMonitor::Fault;
    const QList<Fault> faults = {
        Fault::None, Fault::Sample, Fault::Output, Fault::Limit, Fault::FeedbackDirection,
        Fault::NoProgress, Fault::RunTimeout, Fault::SequenceLate, Fault::SequenceTarget,
    };
    FaultStatusMonitor monitor;
    monitor.setEnabled(true);
    for (int code = 0; code < faults.size(); ++code) {
        QVERIFY(monitor.handleResponse(statusResponse(code, 2)));
        QCOMPARE(monitor.fault(), faults.at(code));
        QCOMPARE(monitor.axisMask(), 2);
        QCOMPARE(monitor.canClear(), code != 0);
    }
    monitor.handleResponse(statusResponse(99));
    QCOMPARE(monitor.fault(), Fault::Other);
    QVERIFY(monitor.canClear());
    QVERIFY(!monitor.handleResponse({{"cmd", "NET GET"}, {"ok", true}}));
    QCOMPARE(monitor.fault(), Fault::Other);
}

void FaultStatusMonitorTests::malformedStatusNeverMeansHealthy()
{
    FaultStatusMonitor monitor;
    monitor.setEnabled(true);
    const QList<QJsonValue> invalidCodes = {QJsonValue(), "0", -1, 1.5, true};
    for (const auto &code : invalidCodes) {
        monitor.handleResponse(statusResponse(1));
        monitor.handleResponse(statusResponse(code));
        QCOMPARE(monitor.fault(), FaultStatusMonitor::Fault::Unknown);
        QVERIFY(!monitor.canClear());
    }
    monitor.handleResponse({{"cmd", "MOTION STATUS"}, {"ok", true}, {"data", QJsonObject{}}});
    QCOMPARE(monitor.fault(), FaultStatusMonitor::Fault::Unknown);
    monitor.handleResponse({{"cmd", "MOTION STATUS"}, {"ok", false}, {"error", "NOT_READY"}});
    QCOMPARE(monitor.fault(), FaultStatusMonitor::Fault::Unknown);
}

void FaultStatusMonitorTests::queriesWaitForResponse()
{
    FaultStatusMonitor monitor;
    QSignalSpy commands(&monitor, &FaultStatusMonitor::commandRequested);
    monitor.setEnabled(true);
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.last().first().toString(), QStringLiteral("!MOTION STATUS"));
    QVERIFY(monitor.isReading());
    QTest::qWait(2200);
    QCOMPARE(commands.size(), 1);
    monitor.handleResponse(statusResponse(0));
    QVERIFY(!monitor.isReading());
    QTRY_COMPARE_WITH_TIMEOUT(commands.size(), 2, 3000);
    QVERIFY(monitor.isReading());
    QTest::qWait(2200);
    QCOMPARE(commands.size(), 2);
}

void FaultStatusMonitorTests::clearRequiresFaultAndVerification()
{
    FaultStatusMonitor monitor;
    QSignalSpy commands(&monitor, &FaultStatusMonitor::commandRequested);
    QSignalSpy results(&monitor, &FaultStatusMonitor::clearFinished);
    monitor.setEnabled(true);
    monitor.clearFault();
    QCOMPARE(commands.size(), 1);
    monitor.handleResponse(statusResponse(0));
    monitor.clearFault();
    QCOMPARE(commands.size(), 1);
    monitor.handleResponse(statusResponse(3, 1));
    monitor.clearFault();
    QCOMPARE(commands.size(), 2);
    QCOMPARE(commands.last().first().toString(), QStringLiteral("!FAULT CLEAR"));
    QVERIFY(monitor.isClearing());
    monitor.clearFault();
    QCOMPARE(commands.size(), 2);
    monitor.handleResponse({{"cmd", "FAULT CLEAR"}, {"ok", true},
                            {"data", QJsonObject{{"cleared", true}}}});
    QCOMPARE(results.size(), 1);
    QCOMPARE(qvariant_cast<FaultStatusMonitor::ClearResult>(results.last().first()),
             FaultStatusMonitor::ClearResult::Succeeded);
    QCOMPARE(commands.last().first().toString(), QStringLiteral("!MOTION STATUS"));
    QCOMPARE(monitor.fault(), FaultStatusMonitor::Fault::Unknown);
    QVERIFY(!monitor.canClear());
    monitor.handleResponse(statusResponse(1));
    QCOMPARE(monitor.fault(), FaultStatusMonitor::Fault::Sample);
    QVERIFY(monitor.canClear());
    monitor.clearFault();
    monitor.handleResponse({{"cmd", "FAULT CLEAR"}, {"ok", true}, {"data", QJsonObject{}}});
    QCOMPARE(qvariant_cast<FaultStatusMonitor::ClearResult>(results.last().first()),
             FaultStatusMonitor::ClearResult::Failed);
    QCOMPARE(monitor.fault(), FaultStatusMonitor::Fault::Unknown);
}

void FaultStatusMonitorTests::clearFailures_data()
{
    using Result = FaultStatusMonitor::ClearResult;
    QTest::addColumn<QString>("error");
    QTest::addColumn<Result>("result");
    QTest::newRow("busy") << QStringLiteral("BUSY") << Result::Busy;
    QTest::newRow("other owner") << QStringLiteral("NOT_OWNER") << Result::NotOwner;
    QTest::newRow("permission") << QStringLiteral("NOT_AUTHORIZED") << Result::NotAuthorized;
    QTest::newRow("feedback") << QStringLiteral("NOT_READY") << Result::FaultPersists;
    QTest::newRow("hardware") << QStringLiteral("HARDWARE_FAULT") << Result::FaultPersists;
    QTest::newRow("output") << QStringLiteral("IO") << Result::FaultPersists;
    QTest::newRow("timeout") << QStringLiteral("TIMEOUT") << Result::TimedOut;
    QTest::newRow("unknown") << QStringLiteral("FUTURE_ERROR") << Result::Failed;
}

void FaultStatusMonitorTests::clearFailures()
{
    QFETCH(QString, error);
    QFETCH(FaultStatusMonitor::ClearResult, result);
    FaultStatusMonitor monitor;
    QSignalSpy commands(&monitor, &FaultStatusMonitor::commandRequested);
    QSignalSpy results(&monitor, &FaultStatusMonitor::clearFinished);
    monitor.setEnabled(true);
    monitor.handleResponse(statusResponse(1));
    monitor.clearFault();
    monitor.handleResponse({{"cmd", "FAULT CLEAR"}, {"ok", false}, {"error", error}});
    QCOMPARE(qvariant_cast<FaultStatusMonitor::ClearResult>(results.last().first()), result);
    QCOMPARE(commands.last().first().toString(), QStringLiteral("!MOTION STATUS"));
    QVERIFY(!monitor.canClear());
    monitor.handleResponse(statusResponse(1));
    QVERIFY(monitor.canClear());
}

void FaultStatusMonitorTests::pauseAndDisconnectCancelOperations()
{
    FaultStatusMonitor monitor;
    QSignalSpy commands(&monitor, &FaultStatusMonitor::commandRequested);
    monitor.setEnabled(true);
    monitor.setPaused(true);
    monitor.handleResponse(statusResponse(1));
    QVERIFY(!monitor.canClear());
    monitor.clearFault();
    QTest::qWait(2200);
    QCOMPARE(commands.size(), 1);
    monitor.setPaused(false);
    QCOMPARE(commands.size(), 2);
    monitor.handleResponse(statusResponse(1));
    monitor.clearFault();
    QVERIFY(monitor.isClearing());
    monitor.setEnabled(false);
    QVERIFY(!monitor.isClearing());
    QVERIFY(!monitor.isReading());
    QVERIFY(!monitor.canClear());
    monitor.handleResponse(statusResponse(1));
    monitor.handleResponse({{"cmd", "FAULT CLEAR"}, {"ok", true},
                            {"data", QJsonObject{{"cleared", true}}}});
    QCOMPARE(monitor.fault(), FaultStatusMonitor::Fault::Unknown);
    QTest::qWait(2200);
    QCOMPARE(commands.size(), 3);
    monitor.setEnabled(true);
    QCOMPARE(commands.size(), 4);
    monitor.setPaused(true);
    monitor.setEnabled(false);
    monitor.setEnabled(true);
    QCOMPARE(commands.size(), 5);
}

QTEST_GUILESS_MAIN(FaultStatusMonitorTests)
#include "FaultStatusMonitorTests.moc"
