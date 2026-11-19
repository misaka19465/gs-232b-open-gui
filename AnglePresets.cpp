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

#include "AnglePresets.h"
#include "AzimuthScale.h"

#include <QApplication>
#include <QFocusEvent>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedLayout>
#include <QToolTip>

#include <utility>

AnglePresets::AnglePresets(AppSettings::Axis axis, QWidget *parent)
    : QWidget(parent), m_axis(axis)
{
    AppSettings settings;
    const bool azimuth = axis == AppSettings::Axis::Azimuth;
    const QString prefix = azimuth ? QStringLiteral("azimuthPreset")
                                   : QStringLiteral("elevationPreset");
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    for (int index = 0; index < AppSettings::presetCount(axis); ++index) {
        auto *cell = new QWidget(this);
        cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto *stack = new QStackedLayout(cell);
        stack->setContentsMargins(0, 0, 0, 0);
        auto *button = new QPushButton(cell);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        button->setObjectName(prefix + QString::number(index));
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        button->setToolTip(tr("Right-click to edit this preset."));
        auto *editor = new QLineEdit(cell);
        editor->setObjectName(prefix + QStringLiteral("Editor") + QString::number(index));
        editor->setAlignment(Qt::AlignCenter);
        editor->setMaxLength(64);
        editor->setAccessibleName(azimuth ? tr("Edit azimuth preset") : tr("Edit elevation preset"));
        stack->addWidget(button);
        stack->addWidget(editor);
        m_presets.push_back({settings.presetAngle(axis, index), cell, button, editor, stack});
        connect(button, &QPushButton::clicked, this, [this, index] {
            emit angleRequested(m_presets[index].angle);
        });
        connect(button, &QPushButton::customContextMenuRequested, this,
                [this, index] { beginEditing(index); });
        connect(editor, &QLineEdit::textChanged, this, [this, index] {
            if (m_activeIndex == index) {
                m_presets[index].editor->setToolTip(editingHint());
                m_presets[index].editor->setAccessibleDescription(editingHint());
                QToolTip::hideText();
            }
        });
        grid->addWidget(cell, index / 2, index % 2);
        grid->setRowStretch(index / 2, 1);
    }
    updateAngles();
}

void AnglePresets::setTargetResolver(TargetResolver resolver)
{
    cancelEditing();
    m_targetResolver = std::move(resolver);
}

void AnglePresets::setAzimuthOffset(int degrees)
{
    if (m_axis != AppSettings::Axis::Azimuth || m_offset == degrees)
        return;
    cancelEditing();
    m_offset = degrees;
    updateAngles();
}

QSize AnglePresets::sizeHint() const
{
    return minimumSizeHint();
}

void AnglePresets::setShortestPathEnabled(bool enabled)
{
    if (m_axis != AppSettings::Axis::Azimuth)
        return;
    cancelEditing();
    m_shortestPathEnabled = enabled;
    updateAngles();
}

bool AnglePresets::eventFilter(QObject *watched, QEvent *event)
{
    if (m_activeIndex < 0)
        return QWidget::eventFilter(watched, event);

    QLineEdit *editor = m_presets[m_activeIndex].editor;
    if (watched == editor) {
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Escape) {
                cancelEditing();
                event->accept();
                return true;
            }
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                submitEditing();
                event->accept();
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut &&
            static_cast<QFocusEvent *>(event)->reason() != Qt::PopupFocusReason)
            cancelEditing();
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget != editor && !editor->isAncestorOf(widget))
            cancelEditing();
    }
    return QWidget::eventFilter(watched, event);
}

void AnglePresets::beginEditing(int index)
{
    cancelEditing();
    Preset &preset = m_presets[index];
    const qint64 corrected = qint64(preset.angle) + m_offset;
    const qint64 displayed = m_shortestPathEnabled ? AzimuthScale::normalized(corrected) : corrected;
    preset.editor->setText(QString::number(displayed));
    preset.editor->setToolTip(editingHint());
    preset.editor->setAccessibleDescription(editingHint());
    preset.stack->setCurrentWidget(preset.editor);
    m_activeIndex = index;
    qApp->installEventFilter(this);
    preset.editor->setFocus(Qt::OtherFocusReason);
    preset.editor->selectAll();
}

void AnglePresets::cancelEditing()
{
    finishEditing();
}

void AnglePresets::finishEditing()
{
    if (m_activeIndex < 0)
        return;
    const int index = m_activeIndex;
    m_activeIndex = -1;
    qApp->removeEventFilter(this);
    Preset &preset = m_presets[index];
    preset.stack->setCurrentWidget(preset.button);
    QToolTip::hideText();
}

void AnglePresets::submitEditing()
{
    if (m_activeIndex < 0)
        return;

    const int index = m_activeIndex;
    const QString text = m_presets[index].editor->text().trimmed();
    const qint64 minimum = m_shortestPathEnabled ? 0 : m_offset;
    const qint64 maximum = minimum + (m_axis == AppSettings::Axis::Azimuth
                                          ? AzimuthScale::Maximum : 180);
    qint64 angle = 0;
    bool numeric = false;
    angle = text.toLongLong(&numeric);
    if (numeric && m_shortestPathEnabled && m_axis == AppSettings::Axis::Azimuth)
        angle = AzimuthScale::normalized(angle);

    QString error;
    int mechanical = 0;
    if (numeric && angle >= minimum && angle <= maximum) {
        if (m_axis == AppSettings::Axis::Azimuth) {
            mechanical = m_shortestPathEnabled
                             ? AzimuthScale(m_offset).mechanicalBearing(angle, m_presets[index].angle)
                             : int(angle - m_offset);
        } else {
            mechanical = int(angle);
        }
    } else if (!numeric && QRegularExpression(QStringLiteral("^[+-]?[0-9]+$")).match(text).hasMatch()) {
        error = m_shortestPathEnabled
                    ? tr("Enter an integer angle.")
                    : tr("Enter an angle from %1 to %2°.").arg(minimum).arg(maximum);
    } else if (!numeric && m_axis == AppSettings::Axis::Azimuth && m_targetResolver) {
        const TargetInputResult result = m_targetResolver(text);
        if (result.bearing && *result.bearing >= 0 && *result.bearing < 360)
            mechanical = AzimuthScale(m_offset).mechanicalBearing(*result.bearing, m_presets[index].angle);
        else
            error = result.error.isEmpty() ? tr("Unknown target. Enter a 4, 6 or 8 character grid, callsign or DXCC prefix.")
                                           : result.error;
    } else {
        error = m_shortestPathEnabled
                    ? tr("Enter an integer angle.")
                    : tr("Enter an angle from %1 to %2°.").arg(minimum).arg(maximum);
    }

    if (!error.isEmpty()) {
        QLineEdit *editor = m_presets[index].editor;
        editor->setToolTip(error);
        editor->setAccessibleDescription(error);
        QToolTip::showText(editor->mapToGlobal(QPoint(0, editor->height())), error, editor);
        emit inputRejected(error);
        return;
    }

    finishEditing();
    m_presets[index].angle = mechanical;
    updateAngles();
    if (!AppSettings().setPresetAngle(m_axis, index, mechanical))
        QMessageBox::warning(this, tr("Application settings"),
                             tr("Could not save application settings."));
}

QString AnglePresets::editingHint() const
{
    const qint64 minimum = m_shortestPathEnabled ? 0 : m_offset;
    const qint64 maximum = minimum + (m_axis == AppSettings::Axis::Azimuth
                                          ? AzimuthScale::Maximum : 180);
    if (m_axis == AppSettings::Axis::Azimuth && m_targetResolver) {
        if (m_shortestPathEnabled)
            return tr("Enter an integer angle, grid, callsign or prefix. Callsigns use DXCC coordinates. Press Enter to save or Esc to cancel.");
        return tr("Enter an angle (%1–%2°), grid, callsign or prefix. Callsigns use DXCC coordinates. Press Enter to save or Esc to cancel.")
            .arg(minimum).arg(maximum);
    }
    return tr("Enter an angle from %1 to %2°. Press Enter to save or Esc to cancel.")
        .arg(minimum).arg(maximum);
}

void AnglePresets::updateAngles()
{
    const qint64 minimum = m_shortestPathEnabled ? 0 : m_offset;
    const qint64 maximum = minimum + (m_axis == AppSettings::Axis::Azimuth
                                          ? AzimuthScale::Maximum : 180);
    for (const Preset &preset : m_presets) {
        const qint64 corrected = qint64(preset.angle) + m_offset;
        preset.button->setText(QStringLiteral("%1").arg(
            m_shortestPathEnabled ? qint64(AzimuthScale::normalized(corrected)) : corrected,
            3, 10, QLatin1Char('0')));
        const auto metrics = preset.button->fontMetrics();
        const int width = qMax(metrics.horizontalAdvance(QString::number(minimum)),
                               metrics.horizontalAdvance(QString::number(maximum)));
        const int minimumWidth = qMax(width, metrics.horizontalAdvance(QStringLiteral("000"))) + 24;
        preset.cell->setMinimumWidth(minimumWidth);
        preset.button->setMinimumWidth(minimumWidth);
    }
}
