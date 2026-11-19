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

#include "GreatCircleMap.h"
#include "MaidenheadLocator.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>

namespace {

QByteArray mapPdf(const QColor &color = Qt::blue, bool square = true)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    {
        QPdfWriter writer(&buffer);
        writer.setResolution(72);
        writer.setPageSize(QPageSize(QSizeF(4, square ? 4 : 6), QPageSize::Inch));
        writer.setPageMargins(QMarginsF());
        QPainter painter(&writer);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::red);
        painter.drawEllipse(QPointF(144, 144), 108, 108);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(144, 144), 103, 103);
    }
    return bytes;
}

class MapServer final : public QTcpServer
{
public:
    MapServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, bytes = QByteArray()]() mutable {
                    bytes.append(socket->readAll());
                    const int end = bytes.indexOf("\r\n\r\n");
                    if (end < 0 || socket->property("handled").toBool())
                        return;
                    int length = 0;
                    for (const QByteArray &line : bytes.left(end).split('\n')) {
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                    }
                    if (bytes.size() < end + 4 + length)
                        return;
                    socket->setProperty("handled", true);
                    requests.append(bytes.mid(end + 4, length));
                    pending.append(socket);
                    if (automatic)
                        respond();
                });
            }
        });
        listen(QHostAddress::LocalHost);
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/map").arg(serverPort()));
    }

    void respond()
    {
        if (pending.isEmpty())
            return;
        const auto socket = pending.takeFirst();
        if (!socket || socket->state() != QAbstractSocket::ConnectedState)
            return;
        socket->write("HTTP/1.1 " + QByteArray::number(status) + " Response\r\n"
                      "Content-Type: application/pdf\r\nConnection: close\r\nContent-Length: " +
                      QByteArray::number(body.size()) + "\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    QList<QByteArray> requests;
    QList<QPointer<QTcpSocket>> pending;
    QByteArray body = mapPdf();
    int status = 200;
    bool automatic = true;
};

} // namespace

class GreatCircleMapTests final : public QObject
{
    Q_OBJECT

private slots:
    void gridCenters_data();
    void gridCenters();
    void invalidGridDoesNotRequest();
    void requestsCenterAndReusesCache();
    void coalescesAndCancelsRequests();
    void destroysWhilePending();
    void rejectsInvalidResponses_data();
    void rejectsInvalidResponses();
    void regeneratesCorruptCache();
    void reportsCacheFailure();
};

void GreatCircleMapTests::gridCenters_data()
{
    QTest::addColumn<QString>("grid");
    QTest::addColumn<double>("longitude");
    QTest::addColumn<double>("latitude");
    QTest::newRow("four") << QStringLiteral("PM95") << 139.0 << 35.5;
    QTest::newRow("six") << QStringLiteral("PM95AA") << 138.0 + 1.0 / 24 << 35.0 + 1.0 / 48;
    QTest::newRow("eight") << QStringLiteral("PM95AA12") << 138.0125 << 35.0 + 2.5 / 240;
    QTest::newRow("normalized") << QStringLiteral(" pm95aa12 ") << 138.0125 << 35.0 + 2.5 / 240;
    QTest::newRow("southwest") << QStringLiteral("AA00AA00") << -180.0 + 1.0 / 240
                              << -90.0 + 1.0 / 480;
    QTest::newRow("northeast") << QStringLiteral("RR99XX99") << 180.0 - 1.0 / 240
                              << 90.0 - 1.0 / 480;
}

void GreatCircleMapTests::gridCenters()
{
    QFETCH(QString, grid);
    QFETCH(double, longitude);
    QFETCH(double, latitude);
    const auto center = MaidenheadLocator::center(grid);
    QVERIFY(center);
    QVERIFY(qAbs(center->x() - longitude) < 1e-9);
    QVERIFY(qAbs(center->y() - latitude) < 1e-9);
}

void GreatCircleMapTests::invalidGridDoesNotRequest()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MapServer server;
    QVERIFY(server.isListening());
    GreatCircleMap map(directory.path(), server.url());
    for (const QString &grid : {QString(), QStringLiteral("ZZ99"), QStringLiteral("PM95ß")}) {
        QVERIFY(!MaidenheadLocator::center(grid));
        map.load(grid);
        QVERIFY(!map.isLoading());
        QVERIFY(map.image().isNull());
    }
    QVERIFY(server.requests.isEmpty());
    QVERIFY(QDir(directory.path()).entryList(QDir::Files).isEmpty());
}

void GreatCircleMapTests::requestsCenterAndReusesCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MapServer server;
    QVERIFY(server.isListening());
    QImage expected;
    {
        GreatCircleMap map(directory.path(), server.url());
        map.load(QStringLiteral(" pm95aa12 "));
        QVERIFY(map.isLoading());
        QTRY_VERIFY(!map.isLoading());
        QVERIFY2(map.errorString().isEmpty(), qPrintable(map.errorString()));
        QCOMPARE(server.requests.size(), 1);
        const QUrlQuery form(QString::fromUtf8(server.requests.first()));
        QCOMPARE(form.queryItemValue(QStringLiteral("location")), QStringLiteral("35.01041667,138.01250000"));
        QCOMPARE(form.queryItemValue(QStringLiteral("paper")), QStringLiteral("SQUARE"));
        QCOMPARE(form.queryItemValue(QStringLiteral("distance")), QStringLiteral("20000"));
        QCOMPARE(map.image().size(), QSize(1024, 1024));
        QCOMPARE(map.image().pixelColor(512, 512), QColor(Qt::blue));
        QCOMPARE(map.image().pixelColor(512, 10), QColor(Qt::blue));
        QCOMPARE(map.image().pixelColor(0, 0).alpha(), 0);
        expected = map.image();
        map.load(QStringLiteral("PM95AA12"));
        QVERIFY(!map.isLoading());
        QCOMPARE(map.image().convertToFormat(expected.format()), expected);
        map.clear();
        map.load(QStringLiteral("PM95AA12"));
        QVERIFY(!map.isLoading());
        QCOMPARE(map.image().convertToFormat(expected.format()), expected);
    }
    server.close();
    GreatCircleMap reopened(directory.path(), server.url());
    reopened.load(QStringLiteral("PM95AA12"));
    QVERIFY(!reopened.isLoading());
    QCOMPARE(reopened.image().convertToFormat(expected.format()), expected);
    QCOMPARE(server.requests.size(), 1);
}

void GreatCircleMapTests::coalescesAndCancelsRequests()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MapServer server;
    server.automatic = false;
    GreatCircleMap map(directory.path(), server.url());
    map.load(QStringLiteral("PM95"));
    QTRY_COMPARE(server.requests.size(), 1);
    map.load(QStringLiteral(" pm95 "));
    QCOMPARE(server.requests.size(), 1);
    map.load(QStringLiteral("IO91"));
    QVERIFY(map.image().isNull());
    QTRY_COMPARE(server.requests.size(), 2);
    server.respond();
    QVERIFY(map.image().isNull());
    QVERIFY(map.isLoading());
    server.body = mapPdf(Qt::green);
    server.respond();
    QTRY_VERIFY(!map.isLoading());
    QCOMPARE(map.image().pixelColor(512, 512), QColor(Qt::green));
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("ns6t-v1-PM95.png"))));
    map.load(QStringLiteral("AA00"));
    QTRY_COMPARE(server.requests.size(), 3);
    map.clear();
    server.respond();
    QVERIFY(map.image().isNull());
    QVERIFY(!map.isLoading());
}

void GreatCircleMapTests::destroysWhilePending()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MapServer server;
    server.automatic = false;
    auto *map = new GreatCircleMap(directory.path(), server.url());
    map->load(QStringLiteral("PM95"));
    QTRY_COMPARE(server.pending.size(), 1);
    const QPointer<QTcpSocket> socket = server.pending.first();
    delete map;
    QTRY_VERIFY(!socket || socket->state() == QAbstractSocket::UnconnectedState);
    QVERIFY(QDir(directory.path()).entryList(QDir::Files).isEmpty());
}

void GreatCircleMapTests::rejectsInvalidResponses_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::newRow("server-error") << 500 << mapPdf();
    QTest::newRow("html-error") << 200 << QByteArray("<html>Service unavailable</html>");
    QTest::newRow("broken-pdf") << 200 << QByteArray("%PDF-1.3\nbroken");
    QTest::newRow("changed-page-contract") << 200 << mapPdf(Qt::blue, false);
    QTest::newRow("too-large") << 200 << QByteArray(16 * 1024 * 1024 + 1, 'x');
}

void GreatCircleMapTests::rejectsInvalidResponses()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MapServer server;
    GreatCircleMap map(directory.path(), server.url());
    map.load(QStringLiteral("PM95"));
    QTRY_VERIFY(!map.isLoading());
    QVERIFY(!map.image().isNull());
    server.status = status;
    server.body = body;
    map.load(QStringLiteral("IO91"));
    QVERIFY(map.image().isNull());
    QTRY_VERIFY(!map.isLoading());
    QVERIFY(map.image().isNull());
    QVERIFY(!map.errorString().isEmpty());
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("ns6t-v1-IO91.png"))));
    QVERIFY(QFile::exists(directory.filePath(QStringLiteral("ns6t-v1-PM95.png"))));
}

void GreatCircleMapTests::regeneratesCorruptCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile file(directory.filePath(QStringLiteral("ns6t-v1-PM95.png")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("broken PNG");
    file.close();
    MapServer server;
    GreatCircleMap map(directory.path(), server.url());
    map.load(QStringLiteral("PM95"));
    QTRY_VERIFY(!map.isLoading());
    QCOMPARE(server.requests.size(), 1);
    QVERIFY(map.errorString().isEmpty());
    QCOMPARE(QImage(file.fileName()).size(), QSize(1024, 1024));
}

void GreatCircleMapTests::reportsCacheFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile file(directory.filePath(QStringLiteral("cache")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    MapServer server;
    GreatCircleMap map(file.fileName(), server.url());
    map.load(QStringLiteral("PM95"));
    QTRY_VERIFY(!map.isLoading());
    QVERIFY(!map.image().isNull());
    QVERIFY(!map.errorString().isEmpty());
}

QTEST_MAIN(GreatCircleMapTests)
#include "GreatCircleMapTests.moc"
