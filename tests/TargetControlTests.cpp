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

#include "ControlPanel.h"
#include "AppSettings.h"
#include "weather/WindDirectionOffsetDialog.h"

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TargetControlTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void submitsTargets_data();
    void submitsTargets();
    void rejectedInputCanBeCorrected();
    void stationGridChangesTakeEffect();
    void presetTargetsAreSavedWithoutMoving();
    void presetTargetRequiresStationGrid();
    void requestedBearingUsesCompensationAndNearestReachablePosition();
    void controlModesAreMutuallyExclusive();
    void windDirectionOffsetDialogPersistsAngle();
    void cancellationDoesNotMove();
    void disablingCancelsPendingInput();
    void elevationRemainsNumeric();

private:
    QTemporaryDir m_settingsDirectory;
};

namespace {

QLineEdit *edit(ControlPanel &panel, const QString &objectName, const QString &text)
{
    auto *editor = panel.findChild<QLineEdit *>(objectName);
    editor->setText(text);
    editor->setFocus();
    return editor;
}

} // namespace

void TargetControlTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDirectory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());
}

void TargetControlTests::submitsTargets_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("command");
    QTest::newRow("grid") << QStringLiteral("FN31") << QStringLiteral("M023");
    QTest::newRow("eight-character-grid") << QStringLiteral("PM95AA12") << QStringLiteral("M246");
    QTest::newRow("callsign") << QStringLiteral("ja1abc") << QStringLiteral("M011");
    QTest::newRow("prefix") << QStringLiteral("JA") << QStringLiteral("M011");
    QTest::newRow("long-call") << QStringLiteral("EA8/K1ABC/P") << QStringLiteral("M335");
    QTest::newRow("numeric") << QStringLiteral("405") << QStringLiteral("M045");
    QTest::newRow("upper-limit") << QStringLiteral("450") << QStringLiteral("M090");
    QTest::newRow("north") << QStringLiteral("0") << QStringLiteral("M000");
}

void TargetControlTests::submitsTargets()
{
    QFETCH(QString, input);
    QFETCH(QString, command);
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *editor = edit(panel, QStringLiteral("azimuthInput"), input);
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first().first().toString(), command);
}

void TargetControlTests::rejectedInputCanBeCorrected()
{
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    QSignalSpy errors(&panel, &ControlPanel::inputError);
    auto *editor = panel.findChild<QLineEdit *>(QStringLiteral("azimuthInput"));
    for (const QString input : {QString(), QStringLiteral("Q1ABC"), QStringLiteral("9223372036854775808"),
                               QStringLiteral("PM95AA"), QStringLiteral("JA1ABC/MM")}) {
        editor->setText(input);
        QTest::keyClick(editor, Qt::Key_Return);
        QVERIFY(commands.isEmpty());
        QCOMPARE(errors.size(), 1);
        QVERIFY(!errors.takeFirst().first().toString().isEmpty());
        QCOMPARE(editor->text(), input);
        editor->setText(QStringLiteral("JA"));
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(commands.size(), 1);
        QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M011"));
    }
}

void TargetControlTests::stationGridChangesTakeEffect()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *editor = edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("JA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    panel.setStationGrid(QStringLiteral("PM95AA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M011"));
    editor = edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("PM95AA"));
    panel.setStationGrid(QStringLiteral("FN31"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M335"));
    panel.setStationGrid({});
    editor = edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("JA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    editor->setText(QStringLiteral("360"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M000"));
}

void TargetControlTests::presetTargetsAreSavedWithoutMoving()
{
    QVERIFY(AppSettings().setShortestPathEnabled(true));
    QVERIFY(AppSettings().setAzimuthOffset(0));
    QVERIFY(AppSettings().setPresetAngle(AppSettings::Axis::Azimuth, 0, 0));
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.setPosition(0, 20);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *button = panel.findChild<QPushButton *>(QStringLiteral("azimuthPreset0"));
    auto *editor = panel.findChild<QLineEdit *>(QStringLiteral("azimuthPresetEditor0"));

    button->customContextMenuRequested(QPoint());
    editor->setText(QStringLiteral("FN31"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 23);
    QVERIFY(commands.isEmpty());
    button->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M023"));

    button->customContextMenuRequested(QPoint());
    editor->setText(QStringLiteral("ja1abc"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 11);
    QVERIFY(commands.isEmpty());
    panel.findChild<QCheckBox *>(QStringLiteral("longPath"))->setChecked(true);
    button->customContextMenuRequested(QPoint());
    editor->setText(QStringLiteral("JA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 191);
    QVERIFY(commands.isEmpty());
    button->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M191"));
}

void TargetControlTests::presetTargetRequiresStationGrid()
{
    QVERIFY(AppSettings().setShortestPathEnabled(true));
    QVERIFY(AppSettings().setPresetAngle(AppSettings::Axis::Azimuth, 0, 45));
    ControlPanel panel;
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    QSignalSpy errors(&panel, &ControlPanel::inputError);
    auto *button = panel.findChild<QPushButton *>(QStringLiteral("azimuthPreset0"));
    auto *editor = panel.findChild<QLineEdit *>(QStringLiteral("azimuthPresetEditor0"));
    button->customContextMenuRequested(QPoint());
    editor->setText(QStringLiteral("JA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    QCOMPARE(errors.size(), 1);
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 45);
    QVERIFY(editor->isVisible());
}

void TargetControlTests::requestedBearingUsesCompensationAndNearestReachablePosition()
{
    QVERIFY(AppSettings().setAzimuthOffset(0));
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(400, 0);
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);

    panel.requestAzimuthBearing(10);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M370"));

    panel.setAzimuthOffset(30);
    panel.requestAzimuthBearing(10);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M340"));
}

void TargetControlTests::controlModesAreMutuallyExclusive()
{
    QVERIFY(AppSettings().setWindProtectionEnabled(false));
    ControlPanel panel;
    auto *manual = panel.findChild<QRadioButton *>(QStringLiteral("manualMode"));
    auto *tracking = panel.findChild<QRadioButton *>(QStringLiteral("trackingMode"));
    auto *windProtection = panel.findChild<QRadioButton *>(QStringLiteral("windProtectionMode"));
    QVERIFY(manual);
    QVERIFY(tracking);
    QVERIFY(windProtection);
    QVERIFY(manual->isChecked());
    QVERIFY(!tracking->isChecked());
    QVERIFY(!windProtection->isChecked());

    QSignalSpy trackingChanges(&panel, &ControlPanel::trackingEnabledChanged);
    QSignalSpy windChanges(&panel, &ControlPanel::windProtectionEnabledChanged);
    tracking->click();
    QVERIFY(tracking->isChecked());
    QVERIFY(!manual->isChecked());
    QVERIFY(!windProtection->isChecked());
    QVERIFY(panel.trackingEnabled());
    QVERIFY(!panel.windProtectionEnabled());

    windProtection->click();
    QVERIFY(!tracking->isChecked());
    QVERIFY(!manual->isChecked());
    QVERIFY(windProtection->isChecked());
    QVERIFY(!panel.trackingEnabled());
    QVERIFY(panel.windProtectionEnabled());

    manual->click();
    QVERIFY(manual->isChecked());
    QVERIFY(!tracking->isChecked());
    QVERIFY(!windProtection->isChecked());
    QVERIFY(!panel.trackingEnabled());
    QVERIFY(!panel.windProtectionEnabled());
    QCOMPARE(trackingChanges.size(), 2);
    QCOMPARE(windChanges.size(), 2);

    QVERIFY(AppSettings().setWindProtectionEnabled(true));
    ControlPanel restoredPanel;
    QVERIFY(!restoredPanel.findChild<QRadioButton *>(QStringLiteral("manualMode"))->isChecked());
    QVERIFY(!restoredPanel.findChild<QRadioButton *>(QStringLiteral("trackingMode"))->isChecked());
    QVERIFY(restoredPanel.findChild<QRadioButton *>(QStringLiteral("windProtectionMode"))
                ->isChecked());
    QVERIFY(AppSettings().setWindProtectionEnabled(false));
}

void TargetControlTests::windDirectionOffsetDialogPersistsAngle()
{
    QVERIFY(AppSettings().setWindDirectionOffset(90));
    WindDirectionOffsetDialog dialog;
    auto *offset = dialog.findChild<QSpinBox *>(QStringLiteral("windDirectionOffset"));
    auto *save = dialog.findChild<QPushButton *>(QStringLiteral("saveWindDirectionOffset"));
    QVERIFY(offset);
    QVERIFY(save);
    QCOMPARE(offset->value(), 90);
    offset->setValue(270);
    save->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(AppSettings().windDirectionOffset(), 270);
}

void TargetControlTests::cancellationDoesNotMove()
{
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *input = edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("JA"));
    QTest::keyClick(input, Qt::Key_Escape);
    QVERIFY(commands.isEmpty());
    edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("JA"));
    panel.findChild<QLineEdit *>(QStringLiteral("elevationInput"))->setFocus();
    QCoreApplication::processEvents();
    QVERIFY(commands.isEmpty());
}

void TargetControlTests::disablingCancelsPendingInput()
{
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *editor = edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("JA"));
    panel.setCommandEnabled(false);
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
}

void TargetControlTests::elevationRemainsNumeric()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(405, 20);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *editor = edit(panel, QStringLiteral("elevationInput"), QStringLiteral("JA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    editor->setText(QStringLiteral("90"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first().first().toString(), QStringLiteral("W405 090"));
}

QTEST_MAIN(TargetControlTests)
#include "TargetControlTests.moc"
