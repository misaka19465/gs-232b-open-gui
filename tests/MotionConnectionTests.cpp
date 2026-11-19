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

#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "MainWindow.h"

#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QPointer>
#include <QRadioButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest>

#ifdef Q_OS_UNIX
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#endif

class MotionConnectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void websocketJogStopsBeforeDisconnectAndDoesNotResume();
#ifdef Q_OS_UNIX
    void serialJogStopReachesDeviceBeforeClosing_data();
    void serialJogStopReachesDeviceBeforeClosing();
#endif

private:
    QTemporaryDir m_directory;
    int m_case = 0;
    MainWindow *m_window = nullptr;
    ConnectionDialog *m_connection = nullptr;
};

void MotionConnectionTests::init()
{
    QVERIFY(m_directory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       m_directory.filePath(QString::number(++m_case)));
    m_window = new MainWindow;
    m_connection = m_window->findChild<ConnectionDialog *>();
}

void MotionConnectionTests::cleanup()
{
    delete m_window;
    m_window = nullptr;
}

void MotionConnectionTests::websocketJogStopsBeforeDisconnectAndDoesNotResume()
{
    QWebSocketServer server(QStringLiteral("motion-test"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QPointer<QWebSocket> peer;
    QStringList commands;
    connect(&server, &QWebSocketServer::newConnection, &server, [&] {
        peer = server.nextPendingConnection();
        peer->setParent(&server);
        connect(peer, &QWebSocket::textMessageReceived, &server,
                [&](const QString &message) { commands.append(message); });
    });
    m_window->findChild<QRadioButton *>(QStringLiteral("networkMode"))->setChecked(true);
    m_window->findChild<QLineEdit *>(QStringLiteral("websocketAddress"))
        ->setText(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort()));
    m_connection->connectRequested();
    QTRY_VERIFY(peer && m_connection->isConnected());
    m_window->show();
    QCoreApplication::processEvents();
    auto *button = m_window->findChild<QToolButton *>(QStringLiteral("azimuthClockwise"));
    QTest::mousePress(button, Qt::LeftButton);
    QTRY_VERIFY(commands.contains(QStringLiteral("R\r")));
    m_connection->disconnectRequested();
    QTRY_VERIFY(!m_connection->isConnected());
    QTRY_VERIFY(commands.contains(QStringLiteral("A\r")));
    m_connection->connectRequested();
    QTRY_VERIFY(m_connection->isConnected());
    commands.clear();
    QTest::mouseRelease(button, Qt::LeftButton);
    QTest::qWait(30);
    QVERIFY(!commands.contains(QStringLiteral("R\r")));
    QVERIFY(!commands.contains(QStringLiteral("A\r")));
}

#ifdef Q_OS_UNIX
void MotionConnectionTests::serialJogStopReachesDeviceBeforeClosing_data()
{
    QTest::addColumn<bool>("destroy");
    QTest::newRow("disconnect") << false;
    QTest::newRow("destroy-window") << true;
}

void MotionConnectionTests::serialJogStopReachesDeviceBeforeClosing()
{
    QFETCH(bool, destroy);
    const int masterFd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    QVERIFY(masterFd >= 0);
    QFile master;
    QVERIFY(master.open(masterFd, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle));
    QVERIFY(::grantpt(masterFd) == 0);
    QVERIFY(::unlockpt(masterFd) == 0);
    const char *slave = ::ptsname(masterFd);
    QVERIFY(slave);
    m_window->findChild<QRadioButton *>(QStringLiteral("serialMode"))->setChecked(true);
    m_window->findChild<QComboBox *>(QStringLiteral("serialPort"))
        ->setEditText(QString::fromLocal8Bit(slave));
    m_connection->connectRequested();
    QVERIFY(m_connection->isConnected());
    m_window->show();
    QCoreApplication::processEvents();
    auto *button = m_window->findChild<QToolButton *>(QStringLiteral("azimuthClockwise"));
    QTest::mousePress(button, Qt::LeftButton);
    QByteArray commands;
    const auto readCommands = [&] {
        char buffer[4096];
        const auto count = ::read(masterFd, buffer, sizeof(buffer));
        if (count > 0)
            commands.append(buffer, count);
    };
    QTRY_VERIFY_WITH_TIMEOUT((readCommands(), commands.contains("R\r")), 1000);
    if (destroy) {
        delete m_window;
        m_window = nullptr;
    } else {
        m_connection->disconnectRequested();
        QVERIFY(!m_connection->isConnected());
    }
    QTRY_VERIFY_WITH_TIMEOUT((readCommands(), commands.contains("A\r")), 1000);
}
#endif

QTEST_MAIN(MotionConnectionTests)
#include "MotionConnectionTests.moc"
