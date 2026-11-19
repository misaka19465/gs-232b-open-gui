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
#include "Compass.h"
#include "ControlPanel.h"

#include <QCheckBox>
#include <QFocusEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

class MotionControlTests final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void jogsOnlyWhileHeld_data();
    void jogsOnlyWhileHeld();
    void keyboardJogStopsOnRelease();
    void losingControlStopsJog_data();
    void losingControlStopsJog();
    void bothStopButtonsCancelDeferredMotion_data();
    void bothStopButtonsCancelDeferredMotion();
    void positioningCancelsJogWithoutLateStop();
    void cancelledHoldCannotRestartOnReentry();
    void modeChangesCancelDeferredMotion();
    void longPathOnlyChangesGeographicTargets_data();
    void longPathOnlyChangesGeographicTargets();
    void longPathPreservesMeasurementsPresetsAndElevation();

private:
    QTemporaryDir m_directory;
    int m_case = 0;
};

void MotionControlTests::init()
{
    QVERIFY(m_directory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       m_directory.filePath(QString::number(++m_case)));
}

void MotionControlTests::jogsOnlyWhileHeld_data()
{
    QTest::addColumn<QString>("buttonName");
    QTest::addColumn<QString>("start");
    QTest::addColumn<QString>("stop");
    QTest::newRow("counterclockwise") << QStringLiteral("azimuthCounterclockwise") << QStringLiteral("L") << QStringLiteral("A");
    QTest::newRow("clockwise") << QStringLiteral("azimuthClockwise") << QStringLiteral("R") << QStringLiteral("A");
    QTest::newRow("up") << QStringLiteral("elevationUp") << QStringLiteral("U") << QStringLiteral("E");
    QTest::newRow("down") << QStringLiteral("elevationDown") << QStringLiteral("D") << QStringLiteral("E");
}

void MotionControlTests::jogsOnlyWhileHeld()
{
    QFETCH(QString, buttonName);
    QFETCH(QString, start);
    QFETCH(QString, stop);
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QCoreApplication::processEvents();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *button = panel.findChild<QToolButton *>(buttonName);
    QTest::mousePress(button, Qt::LeftButton);
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first().first().toString(), start);
    QTest::qWait(400);
    QCOMPARE(commands.size(), 1);
    QTest::mouseRelease(button, Qt::LeftButton);
    QCOMPARE(commands.size(), 2);
    QCOMPARE(commands.last().first().toString(), stop);
}

void MotionControlTests::keyboardJogStopsOnRelease()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QCoreApplication::processEvents();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *button = panel.findChild<QToolButton *>(QStringLiteral("azimuthClockwise"));
    button->setFocus();
    QTest::keyPress(button, Qt::Key_Space);
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first().first().toString(), QStringLiteral("R"));
    QTest::keyRelease(button, Qt::Key_Space);
    QCOMPARE(commands.size(), 2);
    QCOMPARE(commands.last().first().toString(), QStringLiteral("A"));
}

void MotionControlTests::losingControlStopsJog_data()
{
    QTest::addColumn<QString>("reason");
    for (const QString &reason : {QStringLiteral("focus"), QStringLiteral("deactivate"),
                                 QStringLiteral("hide"), QStringLiteral("disable"),
                                 QStringLiteral("elevation"), QStringLiteral("cancel"),
                                 QStringLiteral("mode")})
        QTest::newRow(qPrintable(reason)) << reason;
}

void MotionControlTests::losingControlStopsJog()
{
    QFETCH(QString, reason);
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QCoreApplication::processEvents();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    const bool elevation = reason == QStringLiteral("elevation");
    auto *button = panel.findChild<QToolButton *>(elevation ? QStringLiteral("elevationUp")
                                                          : QStringLiteral("azimuthClockwise"));
    QTest::mousePress(button, Qt::LeftButton);
    QCOMPARE(commands.size(), 1);
    if (reason == QStringLiteral("focus")) {
        QFocusEvent event(QEvent::FocusOut);
        QCoreApplication::sendEvent(button, &event);
    } else if (reason == QStringLiteral("deactivate")) {
        QEvent event(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(&panel, &event);
    } else if (reason == QStringLiteral("hide")) {
        panel.hide();
    } else if (reason == QStringLiteral("disable")) {
        panel.setCommandEnabled(false);
    } else if (elevation) {
        panel.setElevationSupported(false);
    } else if (reason == QStringLiteral("mode")) {
        panel.findChild<QRadioButton *>(QStringLiteral("trackingMode"))->setChecked(true);
    } else {
        panel.cancelPendingMotion();
    }
    QCOMPARE(commands.size(), 2);
    QCOMPARE(commands.last().first().toString(), elevation ? QStringLiteral("E") : QStringLiteral("A"));
    QTest::mouseRelease(button, Qt::LeftButton);
    QCOMPARE(commands.size(), 2);
}

void MotionControlTests::bothStopButtonsCancelDeferredMotion_data()
{
    QTest::addColumn<QString>("buttonName");
    QTest::newRow("azimuth") << QStringLiteral("azimuthStopAll");
    QTest::newRow("elevation") << QStringLiteral("elevationStopAll");
}

void MotionControlTests::bothStopButtonsCancelDeferredMotion()
{
    QFETCH(QString, buttonName);
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QCoreApplication::processEvents();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *stop = panel.findChild<QToolButton *>(buttonName);
    panel.findChild<QSlider *>()->setValue(45);
    stop->click();
    QTest::qWait(200);
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("S"));

    auto *jog = panel.findChild<QToolButton *>(QStringLiteral("azimuthClockwise"));
    QTest::mousePress(jog, Qt::LeftButton);
    stop->click();
    QCOMPARE(commands.size(), 3);
    QCOMPARE(commands.at(0).first().toString(), QStringLiteral("R"));
    QCOMPARE(commands.at(1).first().toString(), QStringLiteral("A"));
    QCOMPARE(commands.at(2).first().toString(), QStringLiteral("S"));
    QTest::mouseRelease(jog, Qt::LeftButton);
    QCOMPARE(commands.size(), 3);
}

void MotionControlTests::positioningCancelsJogWithoutLateStop()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(350, 20);
    panel.show();
    QCoreApplication::processEvents();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *button = panel.findChild<QToolButton *>(QStringLiteral("azimuthClockwise"));
    QTest::mousePress(button, Qt::LeftButton);
    panel.findChild<Compass *>()->angleRequested(10);
    QCOMPARE(commands.size(), 3);
    QCOMPARE(commands.at(1).first().toString(), QStringLiteral("A"));
    QCOMPARE(commands.at(2).first().toString(), QStringLiteral("M370"));
    QTest::mouseRelease(button, Qt::LeftButton);
    QCOMPARE(commands.size(), 3);
}

void MotionControlTests::modeChangesCancelDeferredMotion()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    QVERIFY(!panel.trackingEnabled());
    QVERIFY(!panel.longPathEnabled());
    auto *slider = panel.findChild<QSlider *>();
    slider->setValue(45);
    panel.findChild<QRadioButton *>(QStringLiteral("trackingMode"))->setChecked(true);
    QVERIFY(panel.trackingEnabled());
    QTest::qWait(200);
    QVERIFY(commands.isEmpty());
    slider->setValue(60);
    panel.findChild<QCheckBox *>(QStringLiteral("longPath"))->setChecked(true);
    QVERIFY(panel.longPathEnabled());
    QTest::qWait(200);
    QVERIFY(commands.isEmpty());
}

void MotionControlTests::cancelledHoldCannotRestartOnReentry()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QCoreApplication::processEvents();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *button = panel.findChild<QToolButton *>(QStringLiteral("azimuthClockwise"));
    QTest::mousePress(button, Qt::LeftButton);
    panel.cancelPendingMotion();
    QCOMPARE(commands.size(), 2);
    const QPoint center = button->rect().center();
    QMouseEvent move(QEvent::MouseMove, center, button->mapToGlobal(center),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(button, &move);
    QCOMPARE(commands.size(), 2);
    QTest::mouseRelease(button, Qt::LeftButton);
    QCOMPARE(commands.size(), 2);
    QTest::mousePress(button, Qt::LeftButton);
    QCOMPARE(commands.size(), 3);
    QTest::mouseRelease(button, Qt::LeftButton);
    QCOMPARE(commands.size(), 4);
}

void MotionControlTests::longPathOnlyChangesGeographicTargets_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("offset");
    QTest::addColumn<bool>("shortest");
    QTest::addColumn<QString>("shortCommand");
    QTest::addColumn<QString>("longCommand");
    QTest::newRow("numeric") << QStringLiteral("10") << 0 << true << QStringLiteral("M370") << QStringLiteral("M370");
    QTest::newRow("overlap") << QStringLiteral("190") << 0 << true << QStringLiteral("M190") << QStringLiteral("M190");
    QTest::newRow("offset") << QStringLiteral("10") << 30 << true << QStringLiteral("M340") << QStringLiteral("M340");
    QTest::newRow("continuous") << QStringLiteral("405") << 0 << false << QStringLiteral("M405") << QStringLiteral("M405");
    QTest::newRow("no-shortest") << QStringLiteral("190") << 0 << false << QStringLiteral("M190") << QStringLiteral("M190");
    QTest::newRow("callsign") << QStringLiteral("ja1abc") << 0 << true << QStringLiteral("M371") << QStringLiteral("M191");
    QTest::newRow("prefix") << QStringLiteral("JA") << 0 << true << QStringLiteral("M371") << QStringLiteral("M191");
    QTest::newRow("callsign-offset") << QStringLiteral("JA") << 30 << true << QStringLiteral("M341") << QStringLiteral("M161");
    QTest::newRow("grid") << QStringLiteral("FN31") << 0 << true << QStringLiteral("M383") << QStringLiteral("M203");
    QTest::newRow("grid-overlap") << QStringLiteral("PM95AA12") << 0 << true << QStringLiteral("M246") << QStringLiteral("M426");
    QTest::newRow("grid-no-shortest") << QStringLiteral("FN31") << 0 << false << QStringLiteral("M383") << QStringLiteral("M203");
}

void MotionControlTests::longPathOnlyChangesGeographicTargets()
{
    QFETCH(QString, input);
    QFETCH(int, offset);
    QFETCH(bool, shortest);
    QFETCH(QString, shortCommand);
    QFETCH(QString, longCommand);
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setAzimuthOffset(offset);
    panel.setShortestPathEnabled(shortest);
    panel.setPosition(350, 20);
    panel.show();
    QCoreApplication::processEvents();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *editor = panel.findChild<QLineEdit *>(QStringLiteral("azimuthInput"));
    for (const bool enabled : {false, true, false}) {
        panel.findChild<QCheckBox *>(QStringLiteral("longPath"))->setChecked(enabled);
        QVERIFY(commands.isEmpty());
        editor->setText(input);
        editor->setFocus();
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(commands.size(), 1);
        QCOMPARE(commands.takeFirst().first().toString(), enabled ? longCommand : shortCommand);
    }
    QCOMPARE(panel.azimuth(), 350);
    QCOMPARE(panel.elevation(), 20);
}

void MotionControlTests::longPathPreservesMeasurementsPresetsAndElevation()
{
    QVERIFY(AppSettings().setPresetAngle(AppSettings::Axis::Azimuth, 0, 190));
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(350, 20);
    panel.findChild<QCheckBox *>(QStringLiteral("longPath"))->setChecked(true);
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    panel.findChild<QPushButton *>(QStringLiteral("azimuthPreset0"))->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M190"));
    panel.findChild<Compass *>()->angleRequested(190);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M190"));
    panel.findChild<QSlider *>()->setValue(45);
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("W350 045"));
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 190);
    QCOMPARE(panel.azimuth(), 350);
    QCOMPARE(panel.elevation(), 20);
    panel.findChild<QCheckBox *>(QStringLiteral("longPath"))->setChecked(false);
    panel.findChild<QPushButton *>(QStringLiteral("azimuthPreset0"))->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M190"));
}

QTEST_MAIN(MotionControlTests)
#include "MotionControlTests.moc"
