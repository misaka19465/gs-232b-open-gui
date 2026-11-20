#pragma once

#include "HamAlertProtocol.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

/** @brief 通过 HamAlert Telnet 登录并接收 JSON spots；不自动控制旋转器。 */
class HamAlertController final : public QObject
{
    Q_OBJECT

public:
    /** @brief 创建断开状态的客户端。 */
    explicit HamAlertController(QObject *parent = nullptr);
    /** @brief 创建连接到指定 HamAlert-compatible Telnet 端点的客户端。 */
    HamAlertController(const QString &host, quint16 port, QObject *parent = nullptr);

    /** @brief 设置 HamAlert 用户名与专用 Telnet 密码；已启用时立即重连。 */
    void setCredentials(const QString &username, const QString &password);
    /** @brief 启停 Telnet feed；无有效凭据时启用失败。 */
    bool setEnabled(bool enabled);
    /** @return 是否保持启用并自动重连。 */
    bool isEnabled() const { return m_enabled; }
    /** @return 最近一次启用或连接错误。 */
    QString errorString() const { return m_error; }

signals:
    /** @brief 连接阶段或运行状态变更。 */
    void statusChanged(const QString &status);
    /** @brief JSON feed 开始或停止接收告警。 */
    void streamingChanged(bool streaming);
    /** @brief 连接失败或需要重连时发出本地化诊断信息。 */
    void errorOccurred(const QString &message);
    /** @brief 收到一条包含可定位呼号的 HamAlert JSON spot。 */
    void alertReceived(const HamAlert::Alert &alert);

private:
    enum class ProtocolState {
        Disconnected,
        LoginPrompt,
        PasswordPrompt,
        LoginResult,
        JsonModeAck,
        Streaming,
    };

    void startConnection();
    void processInput();
    bool takeLine(QByteArray &line);
    void writeCommand(const QByteArray &command);
    void sendHeartbeat();
    void scheduleReconnect(const QString &status);
    void setStreaming(bool streaming);

    QTcpSocket m_socket;
    QTimer m_reconnectTimer;
    QTimer m_handshakeTimer;
    QTimer m_heartbeatTimer;
    QTimer m_heartbeatTimeoutTimer;
    QByteArray m_input;
    QByteArray m_pendingHeartbeat;
    QString m_host;
    QString m_username;
    QString m_password;
    QString m_error;
    quint16 m_port;
    quint64 m_heartbeatSequence = 0;
    ProtocolState m_protocolState = ProtocolState::Disconnected;
    bool m_enabled = false;
    bool m_streaming = false;
};
