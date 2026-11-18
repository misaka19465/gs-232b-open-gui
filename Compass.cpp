#include "Compass.h"
#include "GreatCircleMap.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

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

void Compass::updateMapStatus()
{
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

void Compass::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QColor ink = palette().color(group, QPalette::Text);
    // NOTE: 朱红指针与海洋底色区分；断开时保留较柔和的红色以便读取当前方位。
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
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath circle;
        circle.addEllipse(bounds);
        painter.setClipPath(circle);
        painter.drawImage(bounds, m_map->image());
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
        // NOTE: 白色描边保证指针在蓝色海洋和白色陆地上均可辨认。
        painter.setPen(QPen(Qt::white, 4.5, Qt::SolidLine, Qt::RoundCap));
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
