#pragma once

#include <QColor>

class QWidget;

bool editorUsesDarkTheme(const QWidget *widget);

struct ChangeHighlightTheme {
    QColor lineBackground;
    QColor defaultText;
    QColor keyword;
    QColor string;
    QColor comment;
    QColor number;
    QColor preprocessor;
};

ChangeHighlightTheme removedChangeTheme(bool dark);
ChangeHighlightTheme addedChangeTheme(bool dark);
QColor paddingLineBackground(bool dark);

struct DiffLineTheme {
    QColor foreground;
    QColor background;
};

DiffLineTheme diffAddedLineTheme(bool dark);
DiffLineTheme diffRemovedLineTheme(bool dark);
