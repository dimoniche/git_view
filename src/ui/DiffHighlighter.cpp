#include "ui/DiffHighlighter.h"

#include "ui/EditorTheme.h"

#include <QColor>
#include <QFont>
#include <QTextCharFormat>
#include <QWidget>

DiffHighlighter::DiffHighlighter(QTextDocument *document, QWidget *editor)
    : QSyntaxHighlighter(document), m_editor(editor)
{
}

void DiffHighlighter::highlightBlock(const QString &text)
{
    const bool dark = editorUsesDarkTheme(m_editor);
    QTextCharFormat format;

    if (text.startsWith(QLatin1String("+++")) || text.startsWith(QLatin1String("---"))
        || text.startsWith(QLatin1String("diff --git")) || text.startsWith(QLatin1String("index "))
        || text.startsWith(QLatin1String("new file")) || text.startsWith(QLatin1String("deleted file"))
        || text.startsWith(QLatin1String("rename"))) {
        format.setForeground(dark ? QColor(0x88, 0x88, 0x88) : QColor(0x55, 0x55, 0x55));
        format.setFontWeight(QFont::Bold);
    } else if (text.startsWith(QLatin1Char('@'))) {
        format.setForeground(QColor(0x35, 0x7a, 0xbd));
    } else if (text.startsWith(QLatin1Char('+')) && !text.startsWith(QLatin1String("+++"))) {
        const DiffLineTheme theme = diffAddedLineTheme(dark);
        format.setForeground(theme.foreground);
        format.setBackground(theme.background);
        format.setFontWeight(QFont::Bold);
    } else if (text.startsWith(QLatin1Char('-')) && !text.startsWith(QLatin1String("---"))) {
        const DiffLineTheme theme = diffRemovedLineTheme(dark);
        format.setForeground(theme.foreground);
        format.setBackground(theme.background);
        format.setFontWeight(QFont::Bold);
    } else {
        return;
    }

    setFormat(0, text.length(), format);
}
