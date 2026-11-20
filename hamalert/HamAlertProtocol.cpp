#include "HamAlertProtocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

QString jsonText(const QJsonValue &value)
{
    if (value.isString())
        return value.toString().trimmed();
    if (value.isDouble()) {
        QString text = QString::number(value.toDouble(), 'f', 6);
        while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
            text.chop(1);
        if (text.endsWith(QLatin1Char('.')))
            text.chop(1);
        return text;
    }
    return {};
}

QString boundedText(const QJsonObject &object, const QString &key, int maximumLength)
{
    const QString value = jsonText(object.value(key));
    return value.size() <= maximumLength ? value : QString();
}

} // namespace

std::optional<HamAlert::Alert> HamAlert::parseAlert(const QByteArray &line)
{
    if (line.isEmpty() || line.size() > 65536)
        return std::nullopt;

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    const QJsonObject object = document.object();
    Alert alert;
    alert.fullCallsign = boundedText(object, QStringLiteral("fullCallsign"), 64);
    alert.callsign = boundedText(object, QStringLiteral("callsign"), 64);
    if (alert.fullCallsign.isEmpty() && alert.callsign.isEmpty())
        return std::nullopt;

    alert.frequency = boundedText(object, QStringLiteral("frequency"), 24);
    alert.band = boundedText(object, QStringLiteral("band"), 16);
    alert.mode = boundedText(object, QStringLiteral("mode"), 32);
    alert.time = boundedText(object, QStringLiteral("time"), 32);
    alert.entity = boundedText(object, QStringLiteral("entity"), 96);
    alert.spotter = boundedText(object, QStringLiteral("spotter"), 64);
    alert.source = boundedText(object, QStringLiteral("source"), 32);
    alert.triggerComment = boundedText(object, QStringLiteral("triggerComment"), 512);
    return alert;
}
