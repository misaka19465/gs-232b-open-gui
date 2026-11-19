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
#include "AzimuthScale.h"
#include "Compass.h"
#include "ControlPanel.h"

#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

#include <limits>

class ShortestPathTests final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void numericTargets_data();
    void numericTargets();
    void equivalentInputsChooseNearestReachablePosition();
    void presetsPreserveStorageAndUseLatestFeedback();
    void presetEditingUsesCircularAngles();
    void changingModeCancelsPendingCommands();
    void elevationKeepsMechanicalPosition();
    void unsupportedElevationCancelsPendingCommands();
    void disabledPreferenceRestoresContinuousTravel();

private:
    QTemporaryDir m_directory;
    int m_case = 0;
};

namespace {

QLineEdit *edit(ControlPanel &panel, const QString &objectName, const QString &text)
{
    auto *editor = panel.findChild<QLineEdit *>(objectName);
    editor->setText(text);
    editor->setFocus();
    return editor;
}

QString command(int angle)
{
    return QStringLiteral("M%1").arg(angle, 3, 10, QLatin1Char('0'));
}

} // namespace

void ShortestPathTests::init()
{
    QVERIFY(m_directory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       m_directory.filePath(QString::number(++m_case)));
}

void ShortestPathTests::numericTargets_data()
{
    QTest::addColumn<int>("offset");
    QTest::addColumn<int>("current");
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("target");
    const auto row = [](const char *name, int offset, int current, const char *input, int target) {
        QTest::newRow(name) << offset << current << QString::fromLatin1(input) << target;
    };
    row("clockwise-across-north", 0, 350, "10", 370);
    row("counter-clockwise-across-north", 0, 370, "350", 350);
    row("lower-position-is-nearer", 0, 0, "405", 45);
    row("upper-position-is-nearer", 0, 400, "45", 405);
    row("same-bearing-at-upper-limit", 0, 450, "90", 450);
    row("lower-limit-prevents-wrapping", 0, 10, "350", 350);
    row("upper-limit-prevents-wrapping", 0, 450, "100", 100);
    row("north-overlap-tie", 0, 180, "0", 0);
    row("east-overlap-tie", 0, 270, "90", 90);
    row("north-beyond-tie", 0, 181, "720", 360);
    row("negative-input", 0, 370, "-10", 350);
    row("many-turns", 0, 350, "+1090", 370);
    row("negative-offset", -30, 350, "-20", 370);
    row("positive-offset", 30, 350, "40", 370);
    row("negative-offset-north", -30, 400, "0", 390);
    row("positive-offset-north", 30, 0, "0", 330);
    row("int-min-offset", std::numeric_limits<int>::min(), 350, "-2147483638", 370);
    row("int-max-offset", std::numeric_limits<int>::max(), 350, "2147483657", 370);
    row("int64-min-input", 0, 350, "-9223372036854775808", 352);
    row("int64-max-input", 0, 350, "9223372036854775807", 367);
    row("fractional", 0, 350, "10.5", -1);
    row("positive-overflow", 0, 350, "9223372036854775808", -1);
    row("negative-overflow", 0, 350, "-9223372036854775809", -1);
}

void ShortestPathTests::numericTargets()
{
    QFETCH(int, offset);
    QFETCH(int, current);
    QFETCH(QString, input);
    QFETCH(int, target);
    ControlPanel panel;
    panel.setAzimuthOffset(offset);
    panel.setPosition(current, 20);
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    QSignalSpy errors(&panel, &ControlPanel::inputError);
    QTest::keyClick(edit(panel, QStringLiteral("azimuthInput"), input), Qt::Key_Return);
    QCOMPARE(commands.size(), target < 0 ? 0 : 1);
    QCOMPARE(errors.size(), target < 0 ? 1 : 0);
    if (target >= 0)
        QCOMPARE(commands.first().first().toString(), command(target));
    QCOMPARE(panel.azimuth(), current);
}

void ShortestPathTests::equivalentInputsChooseNearestReachablePosition()
{
    for (int offset : {0, -30, 30, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        const AzimuthScale scale(offset);
        for (qint64 input : {std::numeric_limits<qint64>::min(), qint64(-720), qint64(-1),
                             qint64(0), qint64(450), std::numeric_limits<qint64>::max()}) {
            for (int current = 0; current <= 450; ++current) {
                const int target = scale.mechanicalBearing(input, current);
                QVERIFY(target >= 0 && target <= 450);
                QCOMPARE(AzimuthScale::normalized(scale.corrected(target)), AzimuthScale::normalized(input));
                for (int other = 0; other <= 450; ++other) {
                    if (AzimuthScale::normalized(scale.corrected(other)) != AzimuthScale::normalized(input))
                        continue;
                    QVERIFY(qAbs(target - current) <= qAbs(other - current));
                    if (qAbs(target - current) == qAbs(other - current))
                        QVERIFY(target <= other);
                }
            }
        }
    }
}

void ShortestPathTests::presetsPreserveStorageAndUseLatestFeedback()
{
    QVERIFY(AppSettings().setPresetAngle(AppSettings::Axis::Azimuth, 0, 450));
    QVERIFY(AppSettings().setAzimuthOffset(-30));
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *button = panel.findChild<QPushButton *>(QStringLiteral("azimuthPreset0"));
    auto *compass = panel.findChild<Compass *>();
    for (int current : {0, 450}) {
        panel.setPosition(current, 20);
        button->click();
        QCOMPARE(commands.takeFirst().first().toString(), command(current == 0 ? 90 : 450));
        compass->angleRequested(60);
        QCOMPARE(commands.takeFirst().first().toString(), command(current == 0 ? 90 : 450));
        QTest::keyClick(edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("JA")), Qt::Key_Return);
        QCOMPARE(commands.takeFirst().first().toString(), command(current == 0 ? 41 : 401));
    }
    panel.setShortestPathEnabled(false);
    panel.setPosition(0, 20);
    button->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M450"));
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 450);
}

void ShortestPathTests::presetEditingUsesCircularAngles()
{
    QVERIFY(AppSettings().setPresetAngle(AppSettings::Axis::Azimuth, 0, 450));
    QVERIFY(AppSettings().setAzimuthOffset(-30));
    ControlPanel panel;
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *button = panel.findChild<QPushButton *>(QStringLiteral("azimuthPreset0"));
    button->customContextMenuRequested(QPoint());
    auto *editor = button->parentWidget()->findChild<QLineEdit *>(QStringLiteral("azimuthPresetEditor0"));
    QCOMPARE(editor->text().toLongLong(), qint64(60));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 450);
    button->customContextMenuRequested(QPoint());
    editor->setText(QStringLiteral("-30"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 360);
    QVERIFY(commands.isEmpty());
}

void ShortestPathTests::changingModeCancelsPendingCommands()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(405, 20);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *editor = edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("10"));
    panel.findChild<QSlider *>()->setValue(60);
    panel.setShortestPathEnabled(false);
    QVERIFY(editor->text().isEmpty());
    QTest::qWait(200);
    QVERIFY(commands.isEmpty());
    QCOMPARE(panel.azimuth(), 405);
    edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("405"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M405"));
    panel.setShortestPathEnabled(true);
    QVERIFY(editor->text().isEmpty());
    QVERIFY(commands.isEmpty());
}

void ShortestPathTests::elevationKeepsMechanicalPosition()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(405, 20);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    QTest::keyClick(edit(panel, QStringLiteral("elevationInput"), QStringLiteral("450")), Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    QTest::keyClick(edit(panel, QStringLiteral("elevationInput"), QStringLiteral("90")), Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("W405 090"));
    panel.findChild<QPushButton *>(QStringLiteral("elevationPreset0"))->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("W405 000"));
    panel.findChild<QSlider *>()->setValue(60);
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("W405 060"));
}

void ShortestPathTests::disabledPreferenceRestoresContinuousTravel()
{
    QVERIFY(AppSettings().setShortestPathEnabled(false));
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    QTest::keyClick(edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("405")), Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M405"));
    QTest::keyClick(edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("451")), Qt::Key_Return);
    QVERIFY(commands.isEmpty());
}

void ShortestPathTests::unsupportedElevationCancelsPendingCommands()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(405, 20);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *slider = panel.findChild<QSlider *>();
    auto *preset = panel.findChild<QPushButton *>(QStringLiteral("elevationPreset0"));
    auto *editor = edit(panel, QStringLiteral("elevationInput"), QStringLiteral("90"));
    slider->setValue(60);
    panel.setElevationSupported(false);
    QTest::keyClick(editor, Qt::Key_Return);
    preset->click();
    slider->setValue(70);
    QTest::qWait(200);
    QVERIFY(commands.isEmpty());
    panel.setElevationSupported(true);
    preset->click();
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first().first().toString(), QStringLiteral("W405 000"));
}

QTEST_MAIN(ShortestPathTests)
#include "ShortestPathTests.moc"
