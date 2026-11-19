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

#include "DxccDatabase.h"

#include <QFile>
#include <QRegularExpression>
#include <QResource>
#include <QStringList>

#include <cmath>

// NOTE: 显式引用资源初始化函数，确保静态库被其他接口链接时也带上离线数据。
static void initializeDxccData()
{
    Q_INIT_RESOURCE(dxcc_data);
}

namespace {

const QRegularExpression CallsignPattern(
    QStringLiteral("\\A(?:[A-Z]{1,3}|[0-9][A-Z]{1,2})[0-9]+[A-Z]+\\z"));
const QRegularExpression DigitsPattern(QStringLiteral("\\A[0-9]+\\z"));
const QRegularExpression DistrictPrefixPattern(
    QStringLiteral("\\A(?:[A-Z]{1,3}|[0-9][A-Z]{1,2})[0-9]+\\z"));

bool isPortableSuffix(const QString &text)
{
    return text == QStringLiteral("P") || text == QStringLiteral("M") ||
           text == QStringLiteral("QRP") || text == QStringLiteral("QRPP");
}

} // namespace

const DxccDatabase &DxccDatabase::instance()
{
    static const DxccDatabase database;
    return database;
}

DxccDatabase::DxccDatabase()
{
    initializeDxccData();
    QFile file(QStringLiteral(":/dxcc/cty.dat"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    const auto entries = QString::fromUtf8(file.readAll()).split(QLatin1Char(';'));
    const QRegularExpression modifiers(QStringLiteral("[({\\[<~]"));
    for (const QString &entry : entries) {
        const QString trimmed = entry.trimmed();
        const auto newline = trimmed.indexOf(QLatin1Char('\n'));
        if (newline < 0)
            continue;
        const auto header = trimmed.left(newline).split(QLatin1Char(':'));
        if (header.size() != 9)
            continue;
        const QString primary = header[7].trimmed().toUpper();
        // NOTE: 带 * 的条目仅属于 WAE，跳过后让其呼号归入实际 DXCC。
        if (primary.startsWith(QLatin1Char('*')))
            continue;
        bool latitudeOk = false;
        bool longitudeOk = false;
        const double latitude = header[4].trimmed().toDouble(&latitudeOk);
        // NOTE: CTY.DAT 采用西经为正，与应用内东经为正的坐标契约相反。
        const double longitude = -header[5].trimmed().toDouble(&longitudeOk);
        if (!latitudeOk || !longitudeOk || !std::isfinite(latitude) ||
            !std::isfinite(longitude) || std::abs(latitude) > 90 || std::abs(longitude) > 180)
            continue;
        const Entity entity{header[0].trimmed(), primary, QPointF(longitude, latitude)};
        m_entities.insert(primary, entity);
        const auto aliases = trimmed.mid(newline + 1).split(QLatin1Char(','));
        for (QString alias : aliases) {
            alias = alias.trimmed().toUpper();
            const auto modifier = alias.indexOf(modifiers);
            if (modifier >= 0)
                alias.truncate(modifier);
            if (alias.isEmpty())
                continue;
            // NOTE: 仅使用实体代表坐标，忽略别名附带的分区或位置覆盖。
            const bool exact = alias.startsWith(QLatin1Char('='));
            if (exact)
                alias.remove(0, 1);
            if (alias == QStringLiteral("VERSION") || alias.startsWith(QStringLiteral("VER20")))
                continue;
            auto &index = exact ? m_callsigns : m_prefixes;
            if (!index.contains(alias))
                index.insert(alias, entity);
        }
    }
}

std::optional<DxccDatabase::Entity> DxccDatabase::findSimple(const QString &text) const
{
    if (const auto exact = m_callsigns.constFind(text); exact != m_callsigns.cend())
        return *exact;
    if (const auto entity = m_entities.constFind(text); entity != m_entities.cend())
        return *entity;
    const bool callsign = CallsignPattern.match(text).hasMatch() ||
                          DistrictPrefixPattern.match(text).hasMatch();
    for (auto length = text.size(); length > 0; --length) {
        const auto prefix = m_prefixes.constFind(text.left(length));
        if (prefix != m_prefixes.cend() &&
            (callsign || length == text.size() || DigitsPattern.match(text.mid(length)).hasMatch()))
            return *prefix;
    }
    return std::nullopt;
}

std::optional<DxccDatabase::Entity> DxccDatabase::find(const QString &callsignOrPrefix) const
{
    const QString trimmed = callsignOrPrefix.trimmed();
    static const QRegularExpression syntax(
        QStringLiteral("\\A[A-Za-z0-9]+(?:/[A-Za-z0-9]+){0,2}\\z"));
    if (trimmed.size() > 64 || !syntax.match(trimmed).hasMatch())
        return std::nullopt;
    const QString text = trimmed.toUpper();
    static const QRegularExpression versionMarker(QStringLiteral("\\AVER[0-9]{8}\\z"));
    if (versionMarker.match(text).hasMatch())
        return std::nullopt;
    QStringList parts = text.split(QLatin1Char('/'));
    if (parts.contains(QStringLiteral("MM")) || parts.contains(QStringLiteral("AM")))
        return std::nullopt;
    if (const auto exact = m_callsigns.constFind(text); exact != m_callsigns.cend())
        return *exact;
    if (const auto entity = m_entities.constFind(text); entity != m_entities.cend())
        return *entity;

    while (parts.size() > 1 && isPortableSuffix(parts.last()))
        parts.removeLast();
    const QString portable = parts.join(QLatin1Char('/'));
    if (const auto exact = m_callsigns.constFind(portable); exact != m_callsigns.cend())
        return *exact;
    if (parts.size() == 1)
        return findSimple(parts.first());
    if (parts.size() != 2)
        return std::nullopt;

    const bool firstIsCall = CallsignPattern.match(parts[0]).hasMatch();
    const bool secondIsCall = CallsignPattern.match(parts[1]).hasMatch();
    if (firstIsCall && parts[1].size() == 1 && DigitsPattern.match(parts[1]).hasMatch()) {
        // NOTE: 数字异地后缀替换原呼号区号，避免 EA3.../8 被误判为西班牙本土。
        QString call = parts[0];
        static const QRegularExpression district(QStringLiteral("[0-9]+(?=[A-Z]+\\z)"));
        call.replace(district, parts[1]);
        return findSimple(call);
    }
    // NOTE: 明确的异地前缀优先；两端都是完整呼号时拒绝猜测目的地。
    if (firstIsCall && !secondIsCall && findSimple(parts[0]))
        return findSimple(parts[1]);
    if (!firstIsCall && secondIsCall && findSimple(parts[1]))
        return findSimple(parts[0]);
    return std::nullopt;
}
