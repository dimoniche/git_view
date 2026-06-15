#pragma once

#include <QObject>
#include <QWidget>

inline QWidget *hiddenTopLevelToRestore(QWidget *context)
{
    for (QWidget *widget = context; widget; widget = widget->parentWidget()) {
        if (!widget->isVisible()) {
            return widget->window();
        }
    }
    return nullptr;
}

inline void connectRestoreHiddenOwner(QWidget *dialog, QWidget *context)
{
    QWidget *restoreWindow = hiddenTopLevelToRestore(context);
    if (!restoreWindow) {
        return;
    }

    QObject::connect(dialog, &QObject::destroyed, restoreWindow, [restoreWindow]() {
        restoreWindow->show();
        restoreWindow->raise();
        restoreWindow->activateWindow();
    });
}
