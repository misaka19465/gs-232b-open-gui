#include "HamAlertController.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

constexpr int ReconnectIntervalMs = 5000;
constexpr int HandshakeTimeoutMs = 15000;
constexpr int HeartbeatIntervalMs = 60000;
constexpr int HeartbeatTimeoutMs = 30000;
constexpr int MaximumInputSize = 65536;

bool containsToken(const QByteArray &text, const QByteArray &token)
{
    return text.toLower().contains(token.toLower());
}

} // namespace

HamAlertController::HamAlertController(QObject *parent)
    : HamAlertController(QStringLiteral("hamalert.org"), 7300, parent)
{
}

HamAlertController::HamAlertController(const QString &host, quint16 port, QObject *parent)
    : QObject(parent), m_host(host), m_port(port)
{
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(ReconnectIntervalMs);
    m_handshakeTimer.setSingleShot(true);
    m_handshakeTimer.setInterval(HandshakeTimeoutMs);
    m_heartbeatTimer.setInterval(HeartbeatIntervalMs);
    m_heartbeatTimeoutTimer.setSingleShot(true);
    m_heartbeatTimeoutTimer.setInterval(HeartbeatTimeoutMs);

    connect(&m_reconnectTimer, &QTimer::timeout, this, &HamAlertController::startConnection);
    connect(&m_handshakeTimer, &QTimer::timeout, this, [this] {
        scheduleReconnect(tr("HamAlert login timed out; retrying."));
    });
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &HamAlertController::sendHeartbeat);
    connect(&m_heartbeatTimeoutTimer, &QTimer::timeout, this, [this] {
        scheduleReconnect(tr("HamAlert heartbeat timed out; retrying."));
    });
    connect(&m_socket, &QTcpSocket::connected, this, [this] {
        m_input.clear();
        m_protocolState = ProtocolState::LoginPrompt;
        emit statusChanged(tr("Connected; signing in to HamAlert..."));
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &HamAlertController::processInput);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this] {
        if (m_enabled && m_protocolState != ProtocolState::Disconnected &&
            !m_reconnectTimer.isActive())
            scheduleReconnect(tr("HamAlert connection closed; retrying."));
    });
    connect(&m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                if (m_enabled && m_protocolState != ProtocolState::Disconnected)
                    scheduleReconnect(tr("HamAlert connection failed: %1")
                                          .arg(m_socket.errorString()));
            });
}

void HamAlertController::setCredentials(const QString &username, const QString &password)
{
    const QString normalizedUsername = username.trimmed();
    const QString normalizedPassword = password.trimmed();
    if (normalizedUsername == m_username && normalizedPassword == m_password)
        return;

    m_username = normalizedUsername;
    m_password = normalizedPassword;
    if (m_enabled) {
        m_reconnectTimer.stop();
        m_handshakeTimer.stop();
        startConnection();
    }
}

bool HamAlertController::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return true;

    if (!enabled) {
        m_enabled = false;
        setStreaming(false);
        m_reconnectTimer.stop();
        m_handshakeTimer.stop();
        m_heartbeatTimer.stop();
        m_heartbeatTimeoutTimer.stop();
        m_protocolState = ProtocolState::Disconnected;
        m_input.clear();
        m_pendingHeartbeat.clear();
        m_socket.abort();
        m_error.clear();
        emit statusChanged(tr("Disabled"));
        return true;
    }

    if (m_username.isEmpty() || m_password.isEmpty()) {
        m_error = tr("Configure a HamAlert username and Telnet password first.");
        emit statusChanged(m_error);
        return false;
    }

    m_error.clear();
    m_enabled = true;
    startConnection();
    return true;
}

void HamAlertController::startConnection()
{
    if (!m_enabled)
        return;

    m_reconnectTimer.stop();
    m_handshakeTimer.start();
    m_heartbeatTimer.stop();
    m_heartbeatTimeoutTimer.stop();
    setStreaming(false);
    m_protocolState = ProtocolState::Disconnected;
    m_input.clear();
    m_pendingHeartbeat.clear();
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();
    m_protocolState = ProtocolState::LoginPrompt;
    emit statusChanged(tr("Connecting to HamAlert..."));
    m_socket.connectToHost(m_host, m_port);
}

void HamAlertController::processInput()
{
    m_input.append(m_socket.readAll());
    if (m_input.size() > MaximumInputSize) {
        scheduleReconnect(tr("HamAlert sent an oversized response; reconnecting."));
        return;
    }

    while (m_enabled) {
        if (m_protocolState == ProtocolState::LoginPrompt) {
            const int prompt = m_input.toLower().indexOf(QByteArrayLiteral("login:"));
            if (prompt < 0)
                return;
            m_input.remove(0, prompt + int(sizeof("login:") - 1));
            m_protocolState = ProtocolState::PasswordPrompt;
            writeCommand(m_username.toUtf8());
            continue;
        }

        if (m_protocolState == ProtocolState::PasswordPrompt) {
            const int prompt = m_input.toLower().indexOf(QByteArrayLiteral("password:"));
            if (prompt < 0)
                return;
            m_input.remove(0, prompt + int(sizeof("password:") - 1));
            m_protocolState = ProtocolState::LoginResult;
            writeCommand(m_password.toUtf8());
            continue;
        }

        QByteArray line;
        if (!takeLine(line))
            return;
        if (m_protocolState == ProtocolState::LoginResult) {
            if (containsToken(line, QByteArrayLiteral("Login failed")) ||
                containsToken(line, QByteArrayLiteral("Database error"))) {
                scheduleReconnect(tr("HamAlert login failed; check the account credentials."));
                return;
            }
            if (containsToken(line, QByteArrayLiteral("this is HamAlert"))) {
                m_protocolState = ProtocolState::JsonModeAck;
                writeCommand(QByteArrayLiteral("set/json"));
            }
            continue;
        }

        if (m_protocolState == ProtocolState::JsonModeAck) {
            if (containsToken(line, QByteArrayLiteral("Operation successful"))) {
                m_protocolState = ProtocolState::Streaming;
                m_handshakeTimer.stop();
                m_heartbeatTimer.start();
                setStreaming(true);
            } else if (containsToken(line, QByteArrayLiteral("Invalid")) ||
                       containsToken(line, QByteArrayLiteral("failed"))) {
                scheduleReconnect(tr("HamAlert JSON mode was rejected; reconnecting."));
                return;
            }
            continue;
        }

        if (m_protocolState == ProtocolState::Streaming) {
            if (!m_pendingHeartbeat.isEmpty() && line == m_pendingHeartbeat) {
                m_pendingHeartbeat.clear();
                m_heartbeatTimeoutTimer.stop();
                continue;
            }
            if (const auto alert = HamAlert::parseAlert(line))
                emit alertReceived(*alert);
        }
    }
}

bool HamAlertController::takeLine(QByteArray &line)
{
    const int newline = m_input.indexOf('\n');
    if (newline < 0)
        return false;
    line = m_input.left(newline);
    m_input.remove(0, newline + 1);
    if (line.endsWith('\r'))
        line.chop(1);
    return true;
}

void HamAlertController::writeCommand(const QByteArray &command)
{
    m_socket.write(command + QByteArrayLiteral("\r\n"));
}

void HamAlertController::sendHeartbeat()
{
    if (!m_enabled || m_protocolState != ProtocolState::Streaming ||
        !m_pendingHeartbeat.isEmpty())
        return;

    m_pendingHeartbeat = QByteArrayLiteral("gs232b-") +
                         QByteArray::number(++m_heartbeatSequence);
    m_heartbeatTimeoutTimer.start();
    writeCommand(QByteArrayLiteral("echo ") + m_pendingHeartbeat);
}

void HamAlertController::scheduleReconnect(const QString &status)
{
    if (!m_enabled || m_reconnectTimer.isActive())
        return;

    setStreaming(false);
    m_protocolState = ProtocolState::Disconnected;
    m_input.clear();
    m_handshakeTimer.stop();
    m_heartbeatTimer.stop();
    m_heartbeatTimeoutTimer.stop();
    m_pendingHeartbeat.clear();
    m_reconnectTimer.start();
    emit statusChanged(status);
    emit errorOccurred(status);
    m_socket.abort();
}

void HamAlertController::setStreaming(bool streaming)
{
    if (m_streaming == streaming)
        return;

    m_streaming = streaming;
    emit streamingChanged(streaming);
}
