#include "AngleReading.h"
#include "AzimuthScale.h"

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QToolTip>

#include <utility>

AngleReading::AngleReading(int maximum, QWidget *parent)
    : QLabel(parent), m_editor(new QLineEdit(this)),
      m_numericValidator(new QRegularExpressionValidator(
          QRegularExpression(QStringLiteral("^[+-]?[0-9]+$")), m_editor)), m_maximum(maximum)
{
    setAlignment(Qt::AlignCenter);
    m_editor->setObjectName(QStringLiteral("angleEditor"));
    m_editor->setAlignment(Qt::AlignCenter);
    // NOTE: 保留符号及超界输入，让提交校验拒绝原值，避免截断后误发另一个合法角度。
    m_editor->setMaxLength(64);
    m_editor->setValidator(m_numericValidator);
    connect(m_editor, &QLineEdit::textChanged, this, [this] {
        m_editor->setToolTip(toolTip());
        m_editor->setAccessibleDescription(toolTip());
        QToolTip::hideText();
    });
    m_editor->hide();
    setMinimumHeight(m_editor->sizeHint().height());
    setAngle(0);
    setEditingEnabled(false);
}

void AngleReading::setRange(qint64 minimum, qint64 maximum, bool circular)
{
    Q_ASSERT(minimum <= maximum);
    Q_ASSERT(!circular || (minimum == 0 && maximum == 359));
    finishEditing(false);
    m_circular = circular;
    m_minimum = minimum;
    m_maximum = maximum;
    setAngle(m_angle);
    setEditingEnabled(m_editingEnabled);
}

void AngleReading::setAngle(qint64 angle)
{
    if (m_circular)
        angle = AzimuthScale::normalized(angle);
    m_angle = qBound(m_minimum, angle, m_maximum);
    setText(QStringLiteral("%1°").arg(m_angle, 3, 10, QLatin1Char('0')));
}

void AngleReading::setEditingEnabled(bool enabled)
{
    m_editingEnabled = enabled;
    if (!enabled)
        finishEditing(false);
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus);
    QString hint;
    if (enabled) {
        if (m_inputResolver)
            hint = m_inputHint;
        else if (m_circular)
            hint = tr("Click to enter an integer angle. Angles wrap at 360°. Press Enter to move or Esc to cancel.");
        else
            hint = tr("Click to enter an angle (%1–%2°). Press Enter to move or Esc to cancel.")
                       .arg(m_minimum).arg(m_maximum);
    }
    setToolTip(hint);
    m_editor->setToolTip(toolTip());
    setAccessibleDescription(toolTip());
}

void AngleReading::setInputResolver(InputResolver resolver, const QString &hint)
{
    finishEditing(false);
    m_inputResolver = std::move(resolver);
    m_inputHint = hint;
    m_editor->setValidator(m_inputResolver ? nullptr : m_numericValidator);
    setEditingEnabled(m_editingEnabled);
}

bool AngleReading::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget != m_editor && !m_editor->isAncestorOf(widget))
            finishEditing(false);
    }
    if (watched == m_editor) {
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter) {
                const bool submit = key != Qt::Key_Escape;
                finishEditing(submit);
                event->accept();
                return true;
            }
        }
        // NOTE: 离开输入框只取消编辑，避免点击其他控制项时额外发送定位命令。
        if (event->type() == QEvent::FocusOut &&
            static_cast<QFocusEvent *>(event)->reason() != Qt::PopupFocusReason)
            finishEditing(false);
    }
    return QLabel::eventFilter(watched, event);
}

void AngleReading::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_editingEnabled &&
        rect().contains(event->pos())) {
        beginEditing();
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

void AngleReading::keyPressEvent(QKeyEvent *event)
{
    if (m_editingEnabled && (event->key() == Qt::Key_Return ||
                            event->key() == Qt::Key_Enter ||
                            event->key() == Qt::Key_Space ||
                            event->key() == Qt::Key_F2)) {
        beginEditing();
        event->accept();
        return;
    }
    QLabel::keyPressEvent(event);
}

void AngleReading::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    m_editor->setGeometry(contentsRect());
}

void AngleReading::beginEditing()
{
    if (!m_editingEnabled || m_editing)
        return;
    m_editing = true;
    // NOTE: 空白区域通常不接收焦点，因此编辑期间同时监听输入框外的点击。
    qApp->installEventFilter(this);
    m_editor->setText(QString::number(m_angle));
    m_editor->setAccessibleName(accessibleName());
    m_editor->setGeometry(contentsRect());
    m_editor->show();
    m_editor->setFocus(Qt::OtherFocusReason);
    m_editor->selectAll();
}

void AngleReading::finishEditing(bool submit)
{
    if (!m_editing)
        return;
    InputResult input;
    if (submit) {
        bool ok = false;
        const QString text = m_editor->text().trimmed();
        qint64 angle = text.toLongLong(&ok);
        if (ok && m_circular)
            angle = AzimuthScale::normalized(angle);
        if (ok && angle >= m_minimum && angle <= m_maximum)
            input.angle = angle;
        else if (!ok && m_inputResolver &&
                 !m_numericValidator->regularExpression().match(text).hasMatch())
            input = m_inputResolver(text);
        if (!input.angle || *input.angle < m_minimum || *input.angle > m_maximum) {
            const QString error = input.error.isEmpty()
                                      ? (m_circular ? tr("Enter an integer angle.")
                                                    : tr("Enter an angle from %1 to %2°.").arg(m_minimum).arg(m_maximum))
                                      : input.error;
            m_editor->setToolTip(error);
            m_editor->setAccessibleDescription(error);
            QToolTip::showText(m_editor->mapToGlobal(QPoint(0, m_editor->height())),
                               error, m_editor);
            emit inputRejected(error);
            return;
        }
    }

    // NOTE: 先结束编辑再移交焦点和隐藏输入框，避免失焦事件重复处理。
    m_editing = false;
    qApp->removeEventFilter(this);
    if (m_editor->hasFocus())
        setFocus(Qt::OtherFocusReason);
    m_editor->hide();
    QToolTip::hideText();
    emit editingFinished();
    if (submit && m_editingEnabled)
        emit angleRequested(*input.angle);
}
