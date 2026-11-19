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

#include "Compass.h"
#include "GreatCircleMap.h"
#include "GreatCircleMapPalette.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

constexpr qreal MinimumLobeWidth = 10.0;
constexpr qreal MaximumLobeWidth = 180.0;

QPointF compassDirection(qreal degrees)
{
    const qreal radians = qDegreesToRadians(degrees - 90.0);
    return {qCos(radians), qSin(radians)};
}

qreal lobeHalfWidth(qreal gain)
{
    // NOTE: 用对称天线的简化近似把增益转换为半功率波束宽度；只作方向参考。
    const qreal directivity = qPow(10.0, gain / 10.0);
    const qreal width = qBound(MinimumLobeWidth, qSqrt(41253.0 / directivity),
                               MaximumLobeWidth);
    return width / 2.0;
}

QPainterPath antennaLobePath(qreal angle, qreal radius, qreal gain)
{
    const qreal halfWidth = lobeHalfWidth(gain);
    const qreal edgeCosine = qCos(qDegreesToRadians(halfWidth));
    const int sampleCount = qMax(12, qCeil(halfWidth * 1.5));
    QPainterPath path;
    path.moveTo(QPointF());
    for (int index = 0; index <= sampleCount; ++index) {
        const qreal relative = -halfWidth + (2.0 * halfWidth * index) / sampleCount;
        const qreal cosine = qCos(qDegreesToRadians(relative));
        const qreal normalized = qBound(qreal(0), (cosine - edgeCosine) / (1.0 - edgeCosine),
                                        qreal(1));
        const qreal distance = radius * qSqrt(normalized);
        path.lineTo(compassDirection(angle + relative) * distance);
    }
    path.closeSubpath();
    return path;
}

} // namespace

Compass::Compass(QWidget *parent) : QWidget(parent), m_map(new GreatCircleMap(this))
{
    setObjectName(QStringLiteral("azimuthCompass"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setAccessibleName(tr("Azimuth compass"));
    connect(m_map, &GreatCircleMap::changed, this, &Compass::updateMapStatus);
}

void Compass::setGreatCircleMapEnabled(bool enabled)
{
    m_mapEnabled = enabled;
    if (enabled)
        m_map->load(m_stationGrid);
    else
        m_map->clear();
    updateMapStatus();
}

void Compass::setStationGrid(const QString &grid)
{
    m_stationGrid = grid.trimmed().toUpper();
    if (m_mapEnabled)
        m_map->load(m_stationGrid);
    updateMapStatus();
}

void Compass::setAntennaGain(double gain)
{
    m_antennaGain = qBound(0.0, gain, 60.0);
    update();
}

void Compass::updateMapStatus()
{
    m_themedMap = {};
    QString description;
    if (m_mapEnabled) {
        if (m_stationGrid.isEmpty()) {
            description = tr("Set your grid in Settings → My grid… to center the map.");
            setToolTip(description);
        }
        else if (!m_map->errorString().isEmpty()) {
            description = m_map->errorString();
            setToolTip(description);
        }
    }
    setAccessibleDescription(description);
    if (!m_map->errorString().isEmpty())
        emit mapError(m_map->errorString());
    update();
}

qreal Compass::radius() const
{
    return qMin(width(), height()) * (m_mapEnabled ? 0.48 : 0.43);
}

void Compass::setAngle(int degrees)
{
    m_angle = degrees;
    update();
}

QSize Compass::sizeHint() const { return QSize(154, 154); }
QSize Compass::minimumSizeHint() const { return QSize(120, 120); }

void Compass::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange)
        m_themedMap = {};
    QWidget::changeEvent(event);
}

void Compass::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QColor ink = palette().color(group, QPalette::Text);
    // NOTE: 用朱红突出实测方位；断开时保留较柔和的红色以便读取当前方位。
    const QColor needle = isEnabled() ? QColor(0xd8, 0x3a, 0x2e) : QColor(0xb8, 0x70, 0x68);
    const QPointF center(width() / 2.0, height() / 2.0);
    const qreal radius = this->radius();
    painter.translate(center);
    if (m_mapEnabled) {
        const QRectF bounds(-radius, -radius, radius * 2, radius * 2);
        if (m_map->image().isNull()) {
            painter.setPen(ink);
            const QString message = m_stationGrid.isEmpty() ? tr("Set your grid\nto show the map")
                : m_map->isLoading() ? tr("Generating map…")
                                    : tr("Map unavailable\nTurn the map off and on to retry");
            painter.drawText(bounds.adjusted(8, 8, -8, -8),
                             Qt::AlignCenter | Qt::TextWordWrap, message);
            return;
        }
        if (m_themedMap.isNull())
            m_themedMap = GreatCircleMapPalette::recolor(m_map->image(), palette());
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath circle;
        circle.addEllipse(bounds);
        painter.setClipPath(circle);
        painter.drawImage(bounds, m_themedMap);
        if (m_antennaGain > 0.0) {
            QColor fill = palette().color(QPalette::Active, QPalette::Highlight);
            fill.setAlpha(82);
            QColor outline = palette().color(QPalette::Active, QPalette::Text);
            outline.setAlpha(220);
            painter.setPen(QPen(outline, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(fill);
            painter.drawPath(antennaLobePath(m_angle % 360, radius * 0.94, m_antennaGain));
        }
    } else {
        painter.setPen(QPen(ink, 1.4));
        painter.drawEllipse(QPointF(), radius, radius);
        for (int degrees = 0; degrees < 360; degrees += 5) {
            const qreal radians = qDegreesToRadians(qreal(degrees - 90));
            const QPointF direction(qCos(radians), qSin(radians));
            const bool major = degrees % 30 == 0;
            painter.setPen(QPen(ink, major ? 1.3 : 0.8));
            painter.drawLine(direction * (radius - (major ? 9 : 4)), direction * radius);
        }
    }

    const qreal radians = qDegreesToRadians(qreal(m_angle % 360 - 90));
    const QPointF tip = QPointF(qCos(radians), qSin(radians)) * radius * 0.63;
    if (m_mapEnabled) {
        painter.setPen(QPen(palette().color(QPalette::Active, QPalette::Base), 4.5,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(), tip);
    }
    painter.setPen(QPen(needle, 2.5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(), tip);
    painter.setBrush(needle);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(), 3, 3);
}

void Compass::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !isEnabled()) {
        QWidget::mousePressEvent(event);
        return;
    }

    // NOTE: 未取得当前网格的地图时不接受选角，避免加载或错误提示区被解释为定位。
    if (m_mapEnabled && m_map->image().isNull())
        return;

    const QPointF offset = event->localPos() - QPointF(width() / 2.0, height() / 2.0);
    if (qSqrt(offset.x() * offset.x() + offset.y() * offset.y()) > radius()) {
        QWidget::mousePressEvent(event);
        return;
    }

    int degrees = qRound(qRadiansToDegrees(qAtan2(offset.x(), -offset.y())));
    if (degrees < 0)
        degrees += 360;
    emit angleRequested(degrees);
}
