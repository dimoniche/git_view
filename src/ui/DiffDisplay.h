#pragma once

#include <QWidget>

inline QString binaryDiffUserMessage(QWidget *widget)
{
    return widget->tr("Binary file — changes cannot be displayed as text.");
}

inline QString equivalentContentDespiteDiffMessage(QWidget *widget)
{
    return widget->tr("No changes in file content.\n"
                      "(Git may report differences in line endings, file mode, or other metadata.)");
}
