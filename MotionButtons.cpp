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

#include "MotionButtons.h"

#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QToolButton>
#include <QVector>
#include <QtMath>

namespace {

enum class Symbol { Counterclockwise, Clockwise, Up, Down, Stop };

QIcon motionIcon(Symbol symbol, const QPalette &palette)
{
    QIcon icon;
    for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
        for (const int size : {24, 48, 72}) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.scale(size / 24.0, size / 24.0);
            const QColor ink = palette.color(mode == QIcon::Normal ? QPalette::Active
                                                                   : QPalette::Disabled,
                                             QPalette::ButtonText);
            painter.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            if (symbol == Symbol::Stop) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(mode == QIcon::Normal ? QColor(0xd8, 0x3a, 0x2e)
                                                      : QColor(0xb8, 0x70, 0x68));
                // NOTE: 用正六边形的精确比例构造上下尖角，保证左右边严格竖直。
                constexpr qreal radius = 11;
                const qreal halfWidth = qSqrt(3.0) * radius / 2;
                painter.translate(12, 12);
                painter.drawPolygon(QPolygonF(QVector<QPointF>{{0, -radius}, {halfWidth, -radius / 2},
                                              {halfWidth, radius / 2}, {0, radius},
                                              {-halfWidth, radius / 2}, {-halfWidth, -radius / 2}}));
            } else if (symbol == Symbol::Up || symbol == Symbol::Down) {
                if (symbol == Symbol::Down) {
                    painter.translate(24, 24);
                    painter.rotate(180);
                }
                painter.drawLine(QPointF(12, 20), QPointF(12, 4));
                painter.drawPolyline(QPolygonF(QVector<QPointF>{{6, 10}, {12, 4}, {18, 10}}));
            } else {
                if (symbol == Symbol::Clockwise) {
                    painter.translate(24, 0);
                    painter.scale(-1, 1);
                }
                QPainterPath arc;
                arc.moveTo(20, 17);
                arc.cubicTo(20, 6, 10, 4, 5, 12);
                painter.drawPath(arc);
                painter.drawPolyline(QPolygonF(QVector<QPointF>{{5, 5}, {5, 12}, {12, 12}}));
            }
            painter.end();
            icon.addPixmap(pixmap, mode);
        }
    }
    return icon;
}

} // namespace

MotionButtons::MotionButtons(Qt::Orientation orientation, QWidget *parent) : QWidget(parent)
{
    const bool vertical = orientation == Qt::Vertical;
    auto *layout = new QBoxLayout(vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight,
                                  this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    const auto addButton = [this, layout, vertical](Symbol symbol, const QString &name,
                                                   const QString &hint) {
        auto *button = new QToolButton(this);
        button->setObjectName(name);
        button->setIcon(motionIcon(symbol, palette()));
        button->setIconSize(QSize(24, 24));
        button->setMinimumSize(34, 34);
        if (!vertical)
            button->setMaximumWidth(48);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        button->setAccessibleName(hint);
        button->setToolTip(hint);
        button->setAutoRepeat(false);
        button->setFocusPolicy(Qt::StrongFocus);
        layout->addWidget(button);
        return button;
    };
    auto *first = addButton(vertical ? Symbol::Up : Symbol::Counterclockwise,
                            vertical ? QStringLiteral("elevationUp") : QStringLiteral("azimuthCounterclockwise"),
                            vertical ? tr("Hold to move up; release to stop")
                                     : tr("Hold to rotate counterclockwise; release to stop"));
    auto *second = addButton(vertical ? Symbol::Down : Symbol::Clockwise,
                             vertical ? QStringLiteral("elevationDown") : QStringLiteral("azimuthClockwise"),
                             vertical ? tr("Hold to move down; release to stop")
                                      : tr("Hold to rotate clockwise; release to stop"));
    auto *stop = addButton(Symbol::Stop, vertical ? QStringLiteral("elevationStopAll")
                                                 : QStringLiteral("azimuthStopAll"),
                           tr("Stop all motion"));
    const auto wireDirection = [this](QToolButton *button, bool positive) {
        button->installEventFilter(this);
        connect(button, &QToolButton::pressed, this, [this, button, positive] {
            if (!button->isEnabled() || m_activeButton == button)
                return;
            if (m_cancelledButton == button) {
                button->setDown(false);
                return;
            }
            cancelMotion();
            m_activeButton = button;
            qApp->installEventFilter(this);
            emit motionStarted(positive);
        });
        connect(button, &QToolButton::released, this, [this, button] {
            if (m_activeButton == button)
                cancelMotion();
        });
    };
    wireDirection(first, vertical);
    wireDirection(second, !vertical);
    connect(stop, &QToolButton::clicked, this, &MotionButtons::stopAllRequested);
}

void MotionButtons::cancelMotion()
{
    if (!m_activeButton)
        return;
    // NOTE: 先清理按住状态，避免停止后迟到的 released 中断后续定位。
    m_cancelledButton = m_activeButton;
    m_activeButton->setDown(false);
    m_activeButton = nullptr;
    qApp->removeEventFilter(this);
    emit motionStopped();
}

bool MotionButtons::eventFilter(QObject *watched, QEvent *event)
{
    const bool keyEvent = event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease;
    const auto *key = keyEvent ? static_cast<QKeyEvent *>(event) : nullptr;
    const auto *button = qobject_cast<QToolButton *>(watched);
    if (button && button->parentWidget() == this && key && key->key() == Qt::Key_Space && key->isAutoRepeat())
        return true;
    if (watched == m_cancelledButton) {
        const bool mouseInput = event->type() == QEvent::MouseButtonPress ||
                                event->type() == QEvent::MouseButtonDblClick ||
                                event->type() == QEvent::MouseButtonRelease;
        if ((mouseInput && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) ||
            (key && key->key() == Qt::Key_Space))
            m_cancelledButton = nullptr;
        // NOTE: QAbstractButton 会在按住后重新移入时再次发 pressed；取消后必须重新按下。
        else if (event->type() == QEvent::MouseMove)
            return true;
    }
    if (m_activeButton) {
        const auto *widget = qobject_cast<QWidget *>(watched);
        const bool ancestor = widget && (widget == this || widget->isAncestorOf(this));
        const bool released = event->type() == QEvent::MouseButtonRelease &&
                              static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton;
        if (released || event->type() == QEvent::ApplicationDeactivate ||
            (event->type() == QEvent::WindowDeactivate && watched == window()) ||
            (event->type() == QEvent::FocusOut && watched == m_activeButton) ||
            (event->type() == QEvent::Hide && (ancestor || watched == m_activeButton)) ||
            (event->type() == QEvent::EnabledChange && !m_activeButton->isEnabled()))
            cancelMotion();
    }
    return QWidget::eventFilter(watched, event);
}
