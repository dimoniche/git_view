#pragma once

#include <QWidget>

inline QString binaryDiffUserMessage(QWidget *widget)
{
    return widget->tr("Binary file — changes cannot be displayed as text.");
}
