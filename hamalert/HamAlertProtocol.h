#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace HamAlert {

/** @brief HamAlert JSON feed 中可显示并用于手动定位的告警字段。 */
struct Alert {
    QString fullCallsign;
    QString callsign;
    QString frequency;
    QString band;
    QString mode;
    QString time;
    QString entity;
    QString spotter;
    QString source;
    QString triggerComment;
};

/** @brief 解析一行 HamAlert JSON spot；非告警行或无呼号的对象返回空值。 */
std::optional<Alert> parseAlert(const QByteArray &line);

} // namespace HamAlert
