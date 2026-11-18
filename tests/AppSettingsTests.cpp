#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <array>
#include <limits>

class AppSettingsTests final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsDoNotWrite();
    void savesImmediatelyAndPreservesOtherKeys();
    void persistsConnectionPreferences();
    void restoresSerialPortFromScan();
    void invalidConnectionPreferencesUseDefaults_data();
    void invalidConnectionPreferencesUseDefaults();
    void readsExistingPresets();
    void persistsPresetBoundaries();
    void invalidValuesUseDefaults_data();
    void invalidValuesUseDefaults();
    void rejectsInvalidPresetWrites();
    void persistsAzimuthOffset();
    void invalidStoredAzimuthOffset_data();
    void invalidStoredAzimuthOffset();
    void persistsAntennaGain();
    void readsShortestPathPreference_data();
    void readsShortestPathPreference();
    void persistsN1mmPreferences();
    void invalidN1mmPreferencesUseDefaults();
    void persistsGridTrackerPreference();
    void persistsWsjtXPreferences();
    void validatesAndPersistsStationGrid_data();
    void validatesAndPersistsStationGrid();
    void clearsStationGrid();
    void invalidStoredGridDoesNotWrite();
    void reportsStorageFailure();
};

void AppSettingsTests::defaultsDoNotWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(settings.serialPort({}).isEmpty());
    QCOMPARE(settings.serialPort({QStringLiteral("COM3"), QStringLiteral("COM7")}), QStringLiteral("COM3"));
    QCOMPARE(settings.baudRate(), 9600);
    QCOMPARE(settings.connectionAddress(), QStringLiteral("192.168.1.50"));
    QVERIFY(!settings.networkMode());
    QVERIFY(!settings.consoleVisible());
    QVERIFY(!settings.alwaysOnTop());
    QVERIFY(!settings.greatCircleMapEnabled());
    QCOMPARE(settings.antennaGain(), 0.0);
    QVERIFY(settings.stationGrid().isEmpty());
    QCOMPARE(settings.azimuthOffset(), 0);
    QVERIFY(settings.shortestPathEnabled());
    QVERIFY(!settings.n1mmEnabled());
    QVERIFY(!settings.gridTrackerEnabled());
    QCOMPARE(settings.n1mmRotorName(), QStringLiteral("GS232B"));
    const std::array<int, 10> azimuth{0, 45, 90, 135, 180, 225, 270, 315, 360, 405};
    const std::array<int, 6> elevation{0, 25, 50, 75, 100, 125};
    QCOMPARE(AppSettings::presetCount(AppSettings::Axis::Azimuth), int(azimuth.size()));
    QCOMPARE(AppSettings::presetCount(AppSettings::Axis::Elevation), int(elevation.size()));
    for (int index = 0; index < int(azimuth.size()); ++index)
        QCOMPARE(settings.presetAngle(AppSettings::Axis::Azimuth, index), azimuth[index]);
    for (int index = 0; index < int(elevation.size()); ++index)
        QCOMPARE(settings.presetAngle(AppSettings::Axis::Elevation, index), elevation[index]);
    QVERIFY(!QFile::exists(fileName));
}

void AppSettingsTests::savesImmediatelyAndPreservesOtherKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    {
        QSettings existing(fileName, QSettings::IniFormat);
        existing.setValue(QStringLiteral("azimuthPreset3"), 123);
        existing.setValue(QStringLiteral("other/value"), QStringLiteral("keep"));
    }

    AppSettings settings(fileName);
    for (bool visible : {true, false}) {
        QVERIFY(settings.setConsoleVisible(visible));
        QVERIFY(settings.setAlwaysOnTop(visible));
        QVERIFY(settings.setGreatCircleMapEnabled(visible));
        QVERIFY(settings.setShortestPathEnabled(visible));
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray content = file.readAll();
        QVERIFY(content.contains(visible ? "consoleVisible=true" : "consoleVisible=false"));
        QVERIFY(content.contains(visible ? "alwaysOnTop=true" : "alwaysOnTop=false"));
        AppSettings reopened(fileName);
        QCOMPARE(reopened.consoleVisible(), visible);
        QCOMPARE(reopened.alwaysOnTop(), visible);
        QCOMPARE(reopened.greatCircleMapEnabled(), visible);
        QCOMPARE(reopened.shortestPathEnabled(), visible);
        QCOMPARE(reopened.presetAngle(AppSettings::Axis::Azimuth, 3), 123);
        QSettings raw(fileName, QSettings::IniFormat);
        QCOMPARE(raw.value(QStringLiteral("other/value")).toString(), QStringLiteral("keep"));
    }
}

void AppSettingsTests::persistsConnectionPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(settings.setPresetAngle(AppSettings::Axis::Azimuth, 3, 123));
    QVERIFY(settings.setSerialPort(QStringLiteral(" COM7 ")));
    QVERIFY(settings.setConnectionAddress(QStringLiteral(" ws://rotor.local:8080 ")));
    for (int baudRate : {1200, 2400, 4800, 9600}) {
        QVERIFY(settings.setBaudRate(baudRate));
        for (bool network : {true, false}) {
            QVERIFY(settings.setNetworkMode(network));
            QFile file(fileName);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QByteArray content = file.readAll();
            QVERIFY(content.contains("serialPort=COM7"));
            QVERIFY(content.contains("address=ws://rotor.local:8080"));
            QVERIFY(content.contains("baudRate=" + QByteArray::number(baudRate)));
            QVERIFY(content.contains(network ? "networkMode=true" : "networkMode=false"));
            AppSettings reopened(fileName);
            QCOMPARE(reopened.serialPort({QStringLiteral("COM3"), QStringLiteral("COM7")}), QStringLiteral("COM7"));
            QCOMPARE(reopened.baudRate(), baudRate);
            QCOMPARE(reopened.connectionAddress(), QStringLiteral("ws://rotor.local:8080"));
            QCOMPARE(reopened.networkMode(), network);
            QCOMPARE(reopened.presetAngle(AppSettings::Axis::Azimuth, 3), 123);
        }
    }
    for (int invalid : {-1, 0, 9601, 115200}) {
        QVERIFY(!settings.setBaudRate(invalid));
        QCOMPARE(AppSettings(fileName).baudRate(), 9600);
    }
    QVERIFY(settings.setConnectionAddress(QString()));
    QVERIFY(AppSettings(fileName).connectionAddress().isEmpty());
}

void AppSettingsTests::restoresSerialPortFromScan()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    QVERIFY(AppSettings(fileName).setSerialPort(QStringLiteral("COM7")));
    {
        AppSettings settings(fileName);
        QCOMPARE(settings.serialPort({QStringLiteral("COM3"), QStringLiteral("COM7")}), QStringLiteral("COM7"));
        QCOMPARE(settings.serialPort({QStringLiteral("COM3"), QStringLiteral("COM9")}), QStringLiteral("COM3"));
        QVERIFY(settings.serialPort({}).isEmpty());
        QVERIFY(settings.setBaudRate(4800));
        QVERIFY(settings.setConnectionAddress(QStringLiteral("192.0.2.10")));
    }
    QSettings raw(fileName, QSettings::IniFormat);
    QCOMPARE(raw.value(QStringLiteral("connection/serialPort")).toString(), QStringLiteral("COM7"));
    AppSettings restarted(fileName);
    QCOMPARE(restarted.serialPort({QStringLiteral("COM9"), QStringLiteral("COM7")}), QStringLiteral("COM7"));
    QVERIFY(restarted.setSerialPort(QString()));
    QCOMPARE(AppSettings(fileName).serialPort({QStringLiteral("COM3"), QStringLiteral("COM7")}), QStringLiteral("COM3"));
}

void AppSettingsTests::invalidConnectionPreferencesUseDefaults_data()
{
    QTest::addColumn<QByteArray>("baudRate");
    for (const QByteArray &value : {QByteArray(), QByteArray("bad"), QByteArray("true"),
                                   QByteArray("-1"), QByteArray("0"), QByteArray("9600.5"),
                                   QByteArray("115200"), QByteArray("2147483648")})
        QTest::newRow(value.isEmpty() ? "empty" : value.constData()) << value;
}

void AppSettingsTests::invalidConnectionPreferencesUseDefaults()
{
    QFETCH(QByteArray, baudRate);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    const QByteArray content = "[connection]\nbaudRate=" + baudRate + "\nnetworkMode=invalid\nserialPort=COM7\n";
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }
    const AppSettings settings(fileName);
    QCOMPARE(settings.baudRate(), 9600);
    QVERIFY(!settings.networkMode());
    QCOMPARE(settings.serialPort({QStringLiteral("COM3")}), QStringLiteral("COM3"));
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), content);
}

void AppSettingsTests::readsExistingPresets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray content("[General]\nazimuthPreset0=450\nelevationPreset5=90\nelevationPreset7=77\n");
        QCOMPARE(file.write(content), content.size());
    }
    AppSettings settings(fileName);
    QCOMPARE(settings.presetAngle(AppSettings::Axis::Azimuth, 0), 450);
    QCOMPARE(settings.presetAngle(AppSettings::Axis::Elevation, 5), 90);
    QVERIFY(settings.setPresetAngle(AppSettings::Axis::Elevation, 5, 180));
    QSettings raw(fileName, QSettings::IniFormat);
    QCOMPARE(raw.value(QStringLiteral("azimuthPreset0")).toInt(), 450);
    QCOMPARE(raw.value(QStringLiteral("elevationPreset5")).toInt(), 180);
    QCOMPARE(raw.value(QStringLiteral("elevationPreset7")).toInt(), 77);
    QCOMPARE(raw.allKeys().size(), 3);
}

void AppSettingsTests::persistsPresetBoundaries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    {
        AppSettings settings(fileName);
        QVERIFY(settings.setConsoleVisible(true));
        for (auto axis : {AppSettings::Axis::Azimuth, AppSettings::Axis::Elevation}) {
            const int maximum = axis == AppSettings::Axis::Azimuth ? 450 : 180;
            for (int index = 0; index < AppSettings::presetCount(axis); ++index)
                QVERIFY(settings.setPresetAngle(axis, index, index % 2 ? maximum : 0));
        }
    }
    AppSettings reopened(fileName);
    QVERIFY(reopened.consoleVisible());
    for (auto axis : {AppSettings::Axis::Azimuth, AppSettings::Axis::Elevation}) {
        const int maximum = axis == AppSettings::Axis::Azimuth ? 450 : 180;
        for (int index = 0; index < AppSettings::presetCount(axis); ++index)
            QCOMPARE(reopened.presetAngle(axis, index), index % 2 ? maximum : 0);
    }
}

void AppSettingsTests::invalidValuesUseDefaults_data()
{
    QTest::addColumn<QByteArray>("azimuth");
    QTest::addColumn<QByteArray>("elevation");
    QTest::addColumn<QByteArray>("console");
    QTest::addColumn<bool>("visible");
    QTest::newRow("empty") << QByteArray() << QByteArray() << QByteArray() << false;
    QTest::newRow("non-numeric") << QByteArray("bad") << QByteArray("true") << QByteArray("bad") << false;
    QTest::newRow("negative") << QByteArray("-1") << QByteArray("-1") << QByteArray("-1") << false;
    QTest::newRow("too-large") << QByteArray("451") << QByteArray("181") << QByteArray("2") << false;
    QTest::newRow("fractional") << QByteArray("90.5") << QByteArray("10.5") << QByteArray("false") << false;
    QTest::newRow("overflow") << QByteArray("4294967296") << QByteArray("4294967296") << QByteArray("0") << false;
    QTest::newRow("bool-string") << QByteArray("bad") << QByteArray("bad") << QByteArray("true") << true;
    QTest::newRow("bool-number") << QByteArray("bad") << QByteArray("bad") << QByteArray("1") << true;
}

void AppSettingsTests::invalidValuesUseDefaults()
{
    QFETCH(QByteArray, azimuth);
    QFETCH(QByteArray, elevation);
    QFETCH(QByteArray, console);
    QFETCH(bool, visible);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    const QByteArray content = "[General]\nazimuthPreset1=" + azimuth +
                               "\nelevationPreset1=" + elevation +
                               "\n[view]\nconsoleVisible=" + console +
                               "\nalwaysOnTop=" + console +
                               "\ngreatCircleMapEnabled=" + console + '\n';
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }
    AppSettings settings(fileName);
    QCOMPARE(settings.presetAngle(AppSettings::Axis::Azimuth, 1), 45);
    QCOMPARE(settings.presetAngle(AppSettings::Axis::Elevation, 1), 25);
    QCOMPARE(settings.consoleVisible(), visible);
    QCOMPARE(settings.alwaysOnTop(), visible);
    QCOMPARE(settings.greatCircleMapEnabled(), visible);
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), content);
}

void AppSettingsTests::rejectsInvalidPresetWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    for (auto axis : {AppSettings::Axis::Azimuth, AppSettings::Axis::Elevation}) {
        const int beyondMaximum = axis == AppSettings::Axis::Azimuth ? 451 : 181;
        QVERIFY(!settings.setPresetAngle(axis, -1, 90));
        QVERIFY(!settings.setPresetAngle(axis, AppSettings::presetCount(axis), 90));
        QVERIFY(!settings.setPresetAngle(axis, 0, -1));
        QVERIFY(!settings.setPresetAngle(axis, 0, beyondMaximum));
        QCOMPARE(settings.presetAngle(axis, -1), 0);
        QCOMPARE(settings.presetAngle(axis, AppSettings::presetCount(axis)), 0);
    }
    QVERIFY(!QFile::exists(fileName));
}

void AppSettingsTests::persistsAzimuthOffset()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(settings.setPresetAngle(AppSettings::Axis::Azimuth, 0, 450));
    for (int offset : {0, -30, 30, -360, 360,
                       std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        QVERIFY(settings.setAzimuthOffset(offset));
        AppSettings reopened(fileName);
        QCOMPARE(reopened.azimuthOffset(), offset);
        QCOMPARE(reopened.presetAngle(AppSettings::Axis::Azimuth, 0), 450);
        QSettings raw(fileName, QSettings::IniFormat);
        QCOMPARE(raw.value(QStringLiteral("rotor/azimuthOffset")).toInt(), offset);
    }
}

void AppSettingsTests::invalidStoredAzimuthOffset_data()
{
    QTest::addColumn<QByteArray>("value");
    for (const QByteArray &value : {QByteArray(), QByteArray("bad"), QByteArray("30.5"),
                                   QByteArray("true"), QByteArray("2147483648"), QByteArray("-2147483649")})
        QTest::newRow(value.isEmpty() ? "empty" : value.constData()) << value;
}

void AppSettingsTests::invalidStoredAzimuthOffset()
{
    QFETCH(QByteArray, value);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    const QByteArray content = "[rotor]\nazimuthOffset=" + value + '\n';
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }
    AppSettings settings(fileName);
    QCOMPARE(settings.azimuthOffset(), 0);
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), content);
}

void AppSettingsTests::persistsAntennaGain()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(settings.setAntennaGain(12.5));
    QCOMPARE(AppSettings(fileName).antennaGain(), 12.5);
    QVERIFY(settings.setAntennaGain(AppSettings::maximumAntennaGain()));
    QCOMPARE(AppSettings(fileName).antennaGain(), AppSettings::maximumAntennaGain());
    QVERIFY(settings.setAntennaGain(AppSettings::minimumAntennaGain()));
    QCOMPARE(AppSettings(fileName).antennaGain(), 0.0);

    for (const double invalid : {-0.1, 60.1, std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::infinity()}) {
        QVERIFY(!settings.setAntennaGain(invalid));
        QCOMPARE(AppSettings(fileName).antennaGain(), 0.0);
    }

    QSettings raw(fileName, QSettings::IniFormat);
    raw.setValue(QStringLiteral("view/antennaGain"), QStringLiteral("not-a-gain"));
    raw.sync();
    QCOMPARE(AppSettings(fileName).antennaGain(), 0.0);
}

void AppSettingsTests::validatesAndPersistsStationGrid_data()
{
    QTest::addColumn<QString>("grid");
    QTest::addColumn<bool>("valid");
    QTest::newRow("four-characters") << QStringLiteral("PM95") << true;
    QTest::newRow("six-characters") << QStringLiteral("PM95AA") << true;
    QTest::newRow("eight-characters") << QStringLiteral("PM95AA12") << true;
    QTest::newRow("minimum") << QStringLiteral("AA00AA00") << true;
    QTest::newRow("maximum") << QStringLiteral("RR99XX99") << true;
    QTest::newRow("lowercase") << QStringLiteral("pm95aa") << true;
    QTest::newRow("trimmed") << QStringLiteral("  pM95aA12\n") << true;
    QTest::newRow("too-short") << QStringLiteral("PM") << false;
    QTest::newRow("odd-length") << QStringLiteral("PM95A") << false;
    QTest::newRow("field-out-of-range") << QStringLiteral("SS00") << false;
    QTest::newRow("subsquare-out-of-range") << QStringLiteral("PM95YA") << false;
    QTest::newRow("non-numeric-square") << QStringLiteral("PM9A") << false;
    QTest::newRow("non-numeric-extension") << QStringLiteral("PM95AAAA") << false;
    QTest::newRow("too-long") << QStringLiteral("PM95AA12AA") << false;
    QTest::newRow("internal-space") << QStringLiteral("PM 95AA") << false;
    QTest::newRow("internal-newline") << QStringLiteral("PM95\nAA") << false;
    QTest::newRow("fullwidth-digits") << QStringLiteral("PM９５") << false;
    QTest::newRow("unicode-case-expansion") << QStringLiteral("PM95ß") << false;
}

void AppSettingsTests::validatesAndPersistsStationGrid()
{
    QFETCH(QString, grid);
    QFETCH(bool, valid);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(settings.setConsoleVisible(true));
    QVERIFY(settings.setStationGrid(QStringLiteral("AA00")));
    QFile before(fileName);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const QByteArray original = before.readAll();
    before.close();

    QCOMPARE(AppSettings::isValidStationGrid(grid), valid);
    QCOMPARE(settings.setStationGrid(grid), valid);
    const QString expected = valid ? grid.trimmed().toUpper() : QStringLiteral("AA00");
    QCOMPARE(settings.stationGrid(), expected);
    AppSettings reopened(fileName);
    QCOMPARE(reopened.stationGrid(), expected);
    QVERIFY(reopened.consoleVisible());
    QFile after(fileName);
    QVERIFY(after.open(QIODevice::ReadOnly));
    const QByteArray content = after.readAll();
    if (valid)
        QVERIFY(content.contains("grid=" + expected.toUtf8()));
    else
        QCOMPARE(content, original);
}

void AppSettingsTests::clearsStationGrid()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    for (const QString &empty : {QString(), QStringLiteral("  ")}) {
        QVERIFY(settings.setStationGrid(QStringLiteral("PM95AA")));
        QVERIFY(!AppSettings::isValidStationGrid(empty));
        QVERIFY(settings.setStationGrid(empty));
        AppSettings reopened(fileName);
        QVERIFY(reopened.stationGrid().isEmpty());
    }
}

void AppSettingsTests::invalidStoredGridDoesNotWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    const QByteArray content("[station]\ngrid=ZZ99AA\n");
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }
    AppSettings settings(fileName);
    QVERIFY(settings.stationGrid().isEmpty());
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), content);
}

void AppSettingsTests::readsShortestPathPreference_data()
{
    QTest::addColumn<QByteArray>("stored");
    QTest::addColumn<bool>("enabled");
    QTest::newRow("empty") << QByteArray() << true;
    QTest::newRow("invalid") << QByteArray("broken") << true;
    QTest::newRow("invalid-number") << QByteArray("2") << true;
    QTest::newRow("true") << QByteArray("true") << true;
    QTest::newRow("false") << QByteArray("false") << false;
    QTest::newRow("one") << QByteArray("1") << true;
    QTest::newRow("zero") << QByteArray("0") << false;
    QTest::newRow("mixed-case") << QByteArray("FaLsE") << false;
}

void AppSettingsTests::readsShortestPathPreference()
{
    QFETCH(QByteArray, stored);
    QFETCH(bool, enabled);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    const QByteArray content = "[rotor]\nshortestPathEnabled=" + stored + '\n';
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }
    QCOMPARE(AppSettings(fileName).shortestPathEnabled(), enabled);
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), content);
}

void AppSettingsTests::persistsN1mmPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(settings.setN1mmEnabled(true));
    QVERIFY(settings.setN1mmRotorName(QStringLiteral(" Tower A ")));
    {
        AppSettings reopened(fileName);
        QVERIFY(reopened.n1mmEnabled());
        QCOMPARE(reopened.n1mmRotorName(), QStringLiteral("Tower A"));
    }
    for (const QString &invalid : {QString(), QStringLiteral("A,B"), QStringLiteral("A@B"),
                                   QStringLiteral("A\nB"), QString(65, QLatin1Char('A'))}) {
        QVERIFY(!settings.setN1mmRotorName(invalid));
        QCOMPARE(AppSettings(fileName).n1mmRotorName(), QStringLiteral("Tower A"));
    }
    QVERIFY(settings.setN1mmEnabled(false));
    QVERIFY(!AppSettings(fileName).n1mmEnabled());
}

void AppSettingsTests::invalidN1mmPreferencesUseDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    const QByteArray content("[n1mm]\nenabled=broken\nrotorName=Bad@Name\n");
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }
    AppSettings settings(fileName);
    QVERIFY(!settings.n1mmEnabled());
    QCOMPARE(settings.n1mmRotorName(), QStringLiteral("GS232B"));
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), content);
}

void AppSettingsTests::persistsGridTrackerPreference()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(settings.setN1mmEnabled(true));
    for (bool enabled : {true, false}) {
        QVERIFY(settings.setGridTrackerEnabled(enabled));
        AppSettings reopened(fileName);
        QCOMPARE(reopened.gridTrackerEnabled(), enabled);
        QVERIFY(reopened.n1mmEnabled());
    }
    QSettings raw(fileName, QSettings::IniFormat);
    raw.setValue(QStringLiteral("gridtracker/enabled"), QStringLiteral("invalid"));
    raw.sync();
    QVERIFY(!AppSettings(fileName).gridTrackerEnabled());
    QCOMPARE(raw.value(QStringLiteral("gridtracker/enabled")).toString(), QStringLiteral("invalid"));
}

void AppSettingsTests::persistsWsjtXPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(!settings.wsjtXEnabled());
    QVERIFY(settings.setWsjtXEnabled(true));
    const AppSettings reopened(fileName);
    QVERIFY(reopened.wsjtXEnabled());
}

void AppSettingsTests::reportsStorageFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    QVERIFY(QDir().mkdir(fileName));
    AppSettings settings(fileName);
    QVERIFY(!settings.setSerialPort(QStringLiteral("COM7")));
    QVERIFY(!settings.setBaudRate(4800));
    QVERIFY(!settings.setConnectionAddress(QStringLiteral("192.0.2.10")));
    QVERIFY(!settings.setNetworkMode(true));
    QVERIFY(!settings.setConsoleVisible(true));
    QVERIFY(!settings.setGreatCircleMapEnabled(true));
    QVERIFY(!settings.setAntennaGain(12.5));
    QVERIFY(!settings.setAzimuthOffset(-30));
    QVERIFY(!settings.setShortestPathEnabled(false));
    QVERIFY(!settings.setN1mmEnabled(true));
    QVERIFY(!settings.setGridTrackerEnabled(true));
    QVERIFY(!settings.setWsjtXEnabled(true));
    QVERIFY(!settings.setN1mmRotorName(QStringLiteral("Tower A")));
    QVERIFY(!settings.setStationGrid(QStringLiteral("PM95AA")));
    QVERIFY(!settings.setPresetAngle(AppSettings::Axis::Azimuth, 0, 90));
}

QTEST_GUILESS_MAIN(AppSettingsTests)
#include "AppSettingsTests.moc"
