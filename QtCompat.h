#pragma once

#include <QMouseEvent>
#include <QtGlobal>

inline QPointF mouseEventPosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}
