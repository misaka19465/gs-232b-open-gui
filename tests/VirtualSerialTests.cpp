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

#include "virtual_serial/CrDelimitedBuffer.h"
#include "virtual_serial/VirtualSerialBridge.h"

#include <QFile>
#include <QSignalSpy>
#include <QtTest>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

class VirtualSerialTests final : public QObject
{
    Q_OBJECT

private slots:
    void crDelimiterIgnoresChunkBoundaries();
#ifdef Q_OS_UNIX
    void ptyBridgeForwardsBothDirections();
    void ptyCanStopDuringRead();
#endif
};

void VirtualSerialTests::crDelimiterIgnoresChunkBoundaries()
{
    CrDelimitedBuffer buffer;
    QCOMPARE(buffer.append(QByteArrayLiteral("A")), QList<QByteArray>{});
    QCOMPARE(buffer.append(QByteArrayLiteral("B\rC\r")),
             (QList<QByteArray>{QByteArrayLiteral("AB\r"), QByteArrayLiteral("C\r")}));
    QCOMPARE(buffer.append(QByteArrayLiteral("\nD\nE")), QList<QByteArray>{});
    QCOMPARE(buffer.pendingSize(), qsizetype(3));
    QCOMPARE(buffer.append(QByteArrayLiteral("F\r")),
             (QList<QByteArray>{QByteArrayLiteral("D\nEF\r")}));
    QCOMPARE(buffer.pendingSize(), qsizetype(0));
}

#ifdef Q_OS_UNIX
void VirtualSerialTests::ptyBridgeForwardsBothDirections()
{
    VirtualSerialBridge bridge;
    QSignalSpy lineSpy(&bridge, &VirtualSerialBridge::webSocketLineReady);
    QVERIFY(lineSpy.isValid());
    QVERIFY(bridge.start());
    QVERIFY(bridge.clientPortName().startsWith(QLatin1Char('/')));

    const QByteArray devicePath = bridge.clientPortName().toLocal8Bit();
    const int clientFd = ::open(devicePath.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    QVERIFY(clientFd >= 0);

    QVERIFY(::write(clientFd, "A", 1) == 1);
    QVERIFY(::write(clientFd, "B\rC", 3) == 3);
    QVERIFY(::write(clientFd, "\r", 1) == 1);
    QTRY_COMPARE_WITH_TIMEOUT(lineSpy.size(), 2, 1000);
    QCOMPARE(lineSpy.at(0).at(0).toByteArray(), QByteArrayLiteral("AB\r"));
    QCOMPARE(lineSpy.at(1).at(0).toByteArray(), QByteArrayLiteral("C\r"));

    QVERIFY(bridge.feedWebSocketBytes(QByteArrayLiteral("reply")));
    QVERIFY(bridge.feedWebSocketBytes(QByteArrayLiteral("1\rreply2\r")));
    QByteArray received;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        char buffer[128];
        const ssize_t count = ::read(clientFd, buffer, sizeof(buffer));
        if (count > 0)
            received.append(buffer, static_cast<qsizetype>(count));
        return received == QByteArrayLiteral("reply1\rreply2\r");
    }(), 1000);

    ::close(clientFd);
}

void VirtualSerialTests::ptyCanStopDuringRead()
{
    VirtualSerialBridge bridge;
    QSignalSpy lines(&bridge, &VirtualSerialBridge::webSocketLineReady);
    QSignalSpy errors(&bridge, &VirtualSerialBridge::errorOccurred);
    connect(&bridge, &VirtualSerialBridge::webSocketLineReady, &bridge, &VirtualSerialBridge::stop);
    QVERIFY(bridge.start());
    const QByteArray path = bridge.clientPortName().toLocal8Bit();
    const int fd = ::open(path.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    QVERIFY(fd >= 0);
    QFile client;
    QVERIFY(client.open(fd, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle));
    QCOMPARE(::write(fd, "C2\rM123\r", 8), 8);
    QTRY_VERIFY(!bridge.isRunning());
    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.first().first().toByteArray(), QByteArrayLiteral("C2\r"));
    QVERIFY(errors.isEmpty());
}
#endif

QTEST_GUILESS_MAIN(VirtualSerialTests)
#include "VirtualSerialTests.moc"
