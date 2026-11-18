#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace WsjtX {

inline constexpr quint16 ControlPort = 2237;

/** @brief WSJT-X/JTDX 共享 Status 消息中的本站和当前 DX 目标字段。 */
struct Status {
    QString ownCallsign;
    QString ownGrid;
    QString dxCallsign;
    QString dxGrid;
};

/** @brief 解析 WSJT-X/JTDX schema 2 或 3 共同字段；其他类型与版本返回空值。 */
std::optional<Status> parseStatusDatagram(const QByteArray &datagram);

} // namespace WsjtX
