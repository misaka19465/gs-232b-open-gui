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

#include "GridTrackerProtocol.h"

#include <QXmlStreamReader>

#include <cmath>

namespace {

bool plainElement(const QXmlStreamReader &xml)
{
    return xml.attributes().isEmpty() && xml.namespaceDeclarations().isEmpty() &&
           xml.namespaceUri().isEmpty() && xml.prefix().isEmpty();
}

std::optional<GridTracker::Request> readRequest(QXmlStreamReader &xml)
{
    GridTracker::Request request;
    QString query;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::EndElement)
            break;
        if (token == QXmlStreamReader::Characters) {
            query += xml.text();
        } else if (token == QXmlStreamReader::StartElement) {
            if (!plainElement(xml))
                return std::nullopt;
            const QString name = xml.name().toString();
            if (name != QStringLiteral("AZIMUTH") && name != QStringLiteral("ELEVATION"))
                return std::nullopt;
            auto &angle = name == QStringLiteral("AZIMUTH") ? request.azimuth : request.elevation;
            if (angle)
                return std::nullopt;
            bool ok = false;
            const double value = xml.readElementText().trimmed().toDouble(&ok);
            const int maximum = name == QStringLiteral("AZIMUTH") ? 360 : 180;
            if (xml.hasError() || !ok || !std::isfinite(value) || value < 0 || value > maximum)
                return std::nullopt;
            angle = qRound(value);
        } else if (token != QXmlStreamReader::Comment) {
            return std::nullopt;
        }
    }
    if (xml.hasError())
        return std::nullopt;
    query = query.trimmed();
    if (request.azimuth || request.elevation)
        return query.isEmpty() ? std::optional<GridTracker::Request>(request) : std::nullopt;
    if (query == QStringLiteral("AZ?"))
        request.kind = GridTracker::Request::Kind::QueryAzimuth;
    else if (query == QStringLiteral("EL?"))
        request.kind = GridTracker::Request::Kind::QueryElevation;
    else
        return std::nullopt;
    return request;
}

} // namespace

std::optional<QList<GridTracker::Request>> GridTracker::parseDatagram(const QByteArray &datagram)
{
    if (datagram.isEmpty() || datagram.size() > 8192)
        return std::nullopt;
    // NOTE: GridTracker 2 将两个 PST 查询放在同一数据报中；增加内部根节点后统一验证完整报文。
    QXmlStreamReader xml(QByteArray("<datagram>") + datagram + "</datagram>");
    if (!xml.readNextStartElement())
        return std::nullopt;
    QList<Request> requests;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() != QStringLiteral("PST") || !plainElement(xml))
                return std::nullopt;
            const auto request = readRequest(xml);
            if (!request)
                return std::nullopt;
            requests.append(*request);
        } else if (token == QXmlStreamReader::Characters && !xml.isWhitespace()) {
            return std::nullopt;
        } else if (token != QXmlStreamReader::Characters && token != QXmlStreamReader::EndElement &&
                   token != QXmlStreamReader::Comment && token != QXmlStreamReader::EndDocument) {
            return std::nullopt;
        }
    }
    return xml.hasError() || requests.isEmpty() ? std::nullopt : std::optional<QList<Request>>(requests);
}
