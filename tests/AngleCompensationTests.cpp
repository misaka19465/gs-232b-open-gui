#include "AnglePresets.h"
#include "AngleReading.h"
#include "AppSettings.h"
#include "AzimuthScale.h"
#include "Compass.h"
#include "ControlPanel.h"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

#include <limits>

class AngleCompensationTests final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void scaleRoundTrips_data();
    void scaleRoundTrips();
    void circularTargetsChooseNearestReachablePosition();
    void numericTargets_data();
    void numericTargets();
    void compassAndCallsignTargets();
    void feedbackAndElevationKeepMechanicalPosition();
    void presetsKeepTheirStoredMechanicalAngles();
    void changingOffsetCancelsPendingInputAndMotion();

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

int normalized(qint64 angle)
{
    return int((angle % 360 + 360) % 360);
}

} // namespace

void AngleCompensationTests::init()
{
    QVERIFY(m_directory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       m_directory.filePath(QString::number(++m_case)));
    QVERIFY(AppSettings().setShortestPathEnabled(false));
}

void AngleCompensationTests::scaleRoundTrips_data()
{
    QTest::addColumn<int>("offset");
    for (int offset : {0, -30, 30, -359, 359, -360, 360, -450, 450, -720, 720,
                       std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        QTest::newRow(qPrintable(QString::number(offset))) << offset;
}

void AngleCompensationTests::scaleRoundTrips()
{
    QFETCH(int, offset);
    const AzimuthScale scale(offset);
    QCOMPARE(scale.minimum(), qint64(offset));
    QCOMPARE(scale.maximum(), qint64(offset) + 450);
    for (int mechanical = 0; mechanical <= 450; ++mechanical) {
        const auto target = scale.mechanical(scale.corrected(mechanical));
        QVERIFY(target);
        QCOMPARE(*target, mechanical);
    }
    for (qint64 outside : {scale.minimum() - 1, scale.maximum() + 1,
                           std::numeric_limits<qint64>::min(), std::numeric_limits<qint64>::max()})
        QVERIFY(!scale.mechanical(outside));
}

void AngleCompensationTests::circularTargetsChooseNearestReachablePosition()
{
    for (int offset : {0, -30, 30, -359, 359, -360, 360, -720, 720,
                       std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        const AzimuthScale scale(offset);
        for (int current : {0, 45, 90, 180, 225, 270, 360, 405, 450}) {
            for (int bearing = 0; bearing < 360; ++bearing) {
                const int target = scale.mechanicalBearing(bearing, current);
                QVERIFY(target >= 0 && target <= 450);
                QCOMPARE(normalized(scale.corrected(target)), bearing);
                QCOMPARE(scale.mechanicalBearing(bearing - 360, current), target);
                QCOMPARE(scale.mechanicalBearing(bearing + 360, current), target);
                for (int other : {target - 360, target + 360}) {
                    if (other < 0 || other > 450)
                        continue;
                    QVERIFY(qAbs(target - current) <= qAbs(other - current));
                    if (qAbs(target - current) == qAbs(other - current))
                        QVERIFY(target < other);
                }
            }
        }
    }
    QCOMPARE(AzimuthScale(-30).mechanicalBearing(330, 180), 0);
    QCOMPARE(AzimuthScale(-30).mechanicalBearing(330, 181), 360);
    QCOMPARE(AzimuthScale(-30).mechanicalBearing(60, 270), 90);
    QCOMPARE(AzimuthScale(-30).mechanicalBearing(60, 271), 450);
}

void AngleCompensationTests::numericTargets_data()
{
    QTest::addColumn<int>("offset");
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("mechanical");
    const auto row = [](const char *name, int offset, const char *input, int target) {
        QTest::newRow(name) << offset << QString::fromLatin1(input) << target;
    };
    row("lower-limit", -30, "-30", 0);
    row("negative", -30, "-1", 29);
    row("north", -30, "0", 30);
    row("normalized-full-scale", -30, "60", 90);
    row("overlap", -30, "330", 360);
    row("second-north", -30, "360", 390);
    row("upper-limit", -30, "420", 450);
    row("explicit-plus", -30, "+420", 450);
    row("below-lower-limit", -30, "-31", -1);
    row("above-upper-limit", -30, "421", -1);
    row("old-upper-limit", -30, "450", -1);
    row("positive-lower-limit", 30, "30", 0);
    row("positive-north", 30, "360", 330);
    row("positive-upper-limit", 30, "480", 450);
    row("positive-below-limit", 30, "29", -1);
    row("positive-above-limit", 30, "481", -1);
    row("negative-turn-lower", -360, "-360", 0);
    row("negative-turn-upper", -360, "90", 450);
    row("positive-turn-lower", 360, "360", 0);
    row("positive-turn-upper", 360, "810", 450);
    row("int-min-lower", std::numeric_limits<int>::min(), "-2147483648", 0);
    row("int-min-upper", std::numeric_limits<int>::min(), "-2147483198", 450);
    row("int-max-lower", std::numeric_limits<int>::max(), "2147483647", 0);
    row("int-max-upper", std::numeric_limits<int>::max(), "2147484097", 450);
    row("fractional", -30, "30.5", -1);
    row("integer-overflow", -30, "9223372036854775808", -1);
}

void AngleCompensationTests::numericTargets()
{
    QFETCH(int, offset);
    QFETCH(QString, input);
    QFETCH(int, mechanical);
    ControlPanel panel;
    panel.setAzimuthOffset(offset);
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    QSignalSpy errors(&panel, &ControlPanel::inputError);
    auto *editor = edit(panel, QStringLiteral("azimuthInput"), input);
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.size(), mechanical < 0 ? 0 : 1);
    QCOMPARE(errors.size(), mechanical < 0 ? 1 : 0);
    if (mechanical >= 0)
        QCOMPARE(commands.first().first().toString(),
                 QStringLiteral("M%1").arg(mechanical, 3, 10, QLatin1Char('0')));
    else
        QVERIFY(editor->isVisible());
}

void AngleCompensationTests::compassAndCallsignTargets()
{
    ControlPanel panel;
    panel.setAzimuthOffset(-30);
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *compass = panel.findChild<Compass *>();
    for (const auto &entry : {qMakePair(0, 0), qMakePair(450, 360)}) {
        panel.setPosition(entry.first, 20);
        compass->angleRequested(330);
        QCOMPARE(commands.takeFirst().first().toString(),
                 QStringLiteral("M%1").arg(entry.second, 3, 10, QLatin1Char('0')));
        QTest::keyClick(edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("JA")), Qt::Key_Return);
        QCOMPARE(commands.takeFirst().first().toString(),
                 entry.first == 0 ? QStringLiteral("M041") : QStringLiteral("M401"));
    }
    panel.setAzimuthOffset(30);
    panel.setPosition(0, 20);
    compass->angleRequested(0);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M330"));
    QTest::keyClick(edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("JA")), Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M341"));
}

void AngleCompensationTests::feedbackAndElevationKeepMechanicalPosition()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setAzimuthOffset(-30);
    panel.setPosition(450, 20);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *editor = edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("420"));
    panel.setPosition(0, 25);
    QCOMPARE(panel.azimuth(), 0);
    QCOMPARE(editor->text().toLongLong(), qint64(420));
    QTest::keyClick(editor, Qt::Key_Escape);
    QTest::keyClick(edit(panel, QStringLiteral("elevationInput"), QStringLiteral("90")), Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("W000 090"));
    panel.setPosition(450, 25);
    panel.findChild<QPushButton *>(QStringLiteral("elevationPreset0"))->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("W450 000"));
    panel.findChild<QSlider *>()->setValue(60);
    QTRY_COMPARE(commands.size(), 1);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("W450 060"));
}

void AngleCompensationTests::presetsKeepTheirStoredMechanicalAngles()
{
    QVERIFY(AppSettings().setPresetAngle(AppSettings::Axis::Azimuth, 0, 450));
    QVERIFY(AppSettings().setAzimuthOffset(-30));
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *button = panel.findChild<QPushButton *>(QStringLiteral("azimuthPreset0"));
    button->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M450"));
    button->customContextMenuRequested(QPoint());
    auto *reading = button->parentWidget()->findChild<AngleReading *>();
    auto *editor = reading->findChild<QLineEdit *>();
    QCOMPARE(editor->text().toLongLong(), qint64(420));
    editor->setText(QStringLiteral("-30"));
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    QCOMPARE(AppSettings().presetAngle(AppSettings::Axis::Azimuth, 0), 0);
    panel.setAzimuthOffset(30);
    button->click();
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M000"));
    button->customContextMenuRequested(QPoint());
    QCOMPARE(editor->text().toLongLong(), qint64(30));
    QTest::keyClick(editor, Qt::Key_Escape);
}

void AngleCompensationTests::changingOffsetCancelsPendingInputAndMotion()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(123, 45);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *editor = edit(panel, QStringLiteral("azimuthInput"), QStringLiteral("300"));
    panel.findChild<QSlider *>()->setValue(60);
    panel.setAzimuthOffset(-30);
    QCOMPARE(panel.azimuth(), 123);
    QCOMPARE(panel.elevation(), 45);
    QVERIFY(editor->text().isEmpty());
    QTest::qWait(200);
    QVERIFY(commands.isEmpty());
    QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("azimuthReading"))->text(), QStringLiteral("093°"));
}

QTEST_MAIN(AngleCompensationTests)
#include "AngleCompensationTests.moc"
