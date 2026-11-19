/*
 * GS-232B Open GUI - rotator controller GUI.
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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "VirtualSerialEndpoint.h"

#include <QSocketNotifier>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

class VirtualSerialEndpoint::Impl
{
public:
    int masterFd = -1;
    int slaveFd = -1;
    QSocketNotifier *readNotifier = nullptr;
    QSocketNotifier *writeNotifier = nullptr;
    QByteArray pendingWrite;
};

namespace {

QString systemError(const char *operation)
{
    return QStringLiteral("%1: %2")
        .arg(QString::fromLatin1(operation), QString::fromLocal8Bit(std::strerror(errno)));
}

bool configureRawTerminal(int fd)
{
    termios settings{};
    if (::tcgetattr(fd, &settings) != 0)
        return false;

    settings.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    settings.c_oflag &= ~OPOST;
    settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    settings.c_cflag &= ~(CSIZE | PARENB);
    settings.c_cflag |= CS8 | CLOCAL | CREAD;
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;
    return ::tcsetattr(fd, TCSANOW, &settings) == 0;
}

} // namespace

VirtualSerialEndpoint::VirtualSerialEndpoint(QObject *parent)
    : QObject(parent), m_impl(std::make_unique<Impl>())
{
}

VirtualSerialEndpoint::~VirtualSerialEndpoint()
{
    stop();
}

bool VirtualSerialEndpoint::start(const VirtualSerialOptions &options)
{
    Q_UNUSED(options);
    stop();

    const int masterFd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (masterFd < 0) {
        emit errorOccurred(systemError("posix_openpt"));
        return false;
    }
    if (::grantpt(masterFd) != 0 || ::unlockpt(masterFd) != 0) {
        const QString error = systemError("grantpt/unlockpt");
        ::close(masterFd);
        emit errorOccurred(error);
        return false;
    }

    const char *slaveName = ::ptsname(masterFd);
    if (!slaveName) {
        const QString error = systemError("ptsname");
        ::close(masterFd);
        emit errorOccurred(error);
        return false;
    }
    const QString clientPort = QString::fromLocal8Bit(slaveName);
    const QByteArray encodedSlaveName = clientPort.toLocal8Bit();
    const int slaveFd = ::open(encodedSlaveName.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (slaveFd < 0) {
        const QString error = systemError("open PTY slave");
        ::close(masterFd);
        emit errorOccurred(error);
        return false;
    }

    if (!configureRawTerminal(slaveFd)) {
        const QString error = systemError("configure PTY slave");
        ::close(slaveFd);
        ::close(masterFd);
        emit errorOccurred(error);
        return false;
    }
    m_impl->masterFd = masterFd;
    m_impl->slaveFd = slaveFd;
    m_impl->readNotifier = new QSocketNotifier(masterFd, QSocketNotifier::Read, this);
    m_impl->writeNotifier = new QSocketNotifier(masterFd, QSocketNotifier::Write, this);
    m_impl->writeNotifier->setEnabled(false);

    connect(m_impl->readNotifier, &QSocketNotifier::activated, this, [this] {
        char buffer[4096];
        for (;;) {
            const ssize_t count = ::read(m_impl->masterFd, buffer, sizeof(buffer));
            if (count > 0) {
                emit bytesReceived(QByteArray(buffer, static_cast<qsizetype>(count)));
                // NOTE: 转发失败可能同步关闭端点，不得继续读取已释放的描述符。
                if (!m_running)
                    return;
                continue;
            }
            if (count < 0 && errno == EINTR)
                continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;
            if (count < 0) {
                const QString error = systemError("read PTY master");
                emit errorOccurred(error);
                stop();
            }
            break;
        }
    });

    connect(m_impl->writeNotifier, &QSocketNotifier::activated, this, [this] {
        while (!m_impl->pendingWrite.isEmpty()) {
            const ssize_t count = ::write(m_impl->masterFd,
                                          m_impl->pendingWrite.constData(),
                                          static_cast<size_t>(m_impl->pendingWrite.size()));
            if (count > 0) {
                m_impl->pendingWrite.remove(0, static_cast<qsizetype>(count));
                continue;
            }
            if (count < 0 && errno == EINTR)
                continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                return;
            const QString error = systemError("write PTY master");
            emit errorOccurred(error);
            stop();
            return;
        }
        m_impl->writeNotifier->setEnabled(false);
    });

    m_clientPortName = clientPort;
    m_running = true;
    emit started(m_clientPortName);
    return true;
}

void VirtualSerialEndpoint::stop()
{
    const bool wasRunning = m_running;
    m_running = false;
    m_clientPortName.clear();
    m_impl->pendingWrite.clear();

    if (m_impl->readNotifier) {
        delete m_impl->readNotifier;
        m_impl->readNotifier = nullptr;
    }
    if (m_impl->writeNotifier) {
        delete m_impl->writeNotifier;
        m_impl->writeNotifier = nullptr;
    }
    if (m_impl->slaveFd >= 0) {
        ::close(m_impl->slaveFd);
        m_impl->slaveFd = -1;
    }
    if (m_impl->masterFd >= 0) {
        ::close(m_impl->masterFd);
        m_impl->masterFd = -1;
    }

    if (wasRunning)
        emit stopped();
}

bool VirtualSerialEndpoint::isRunning() const
{
    return m_running;
}

QString VirtualSerialEndpoint::clientPortName() const
{
    return m_clientPortName;
}

bool VirtualSerialEndpoint::writeToClient(const QByteArray &bytes)
{
    if (!m_running) {
        emit errorOccurred(tr("Virtual serial endpoint is not running"));
        return false;
    }
    if (bytes.isEmpty())
        return true;

    m_impl->pendingWrite.append(bytes);
    while (!m_impl->pendingWrite.isEmpty()) {
        const ssize_t count = ::write(m_impl->masterFd,
                                      m_impl->pendingWrite.constData(),
                                      static_cast<size_t>(m_impl->pendingWrite.size()));
        if (count > 0) {
            m_impl->pendingWrite.remove(0, static_cast<qsizetype>(count));
            continue;
        }
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            m_impl->writeNotifier->setEnabled(true);
            return true;
        }
        emit errorOccurred(systemError("write PTY master"));
        stop();
        return false;
    }
    return true;
}
