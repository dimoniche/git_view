#include "ui/DiffHighlighter.h"

#include <QColor>
#include <QFont>
#include <QTextCharFormat>

DiffHighlighter::DiffHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
}

void DiffHighlighter::highlightBlock(const QString &text)
{
    QTextCharFormat format;

    if (text.startsWith(QLatin1String("+++")) || text.startsWith(QLatin1String("---"))
        || text.startsWith(QLatin1String("diff --git")) || text.startsWith(QLatin1String("index "))
        || text.startsWith(QLatin1String("new file")) || text.startsWith(QLatin1String("deleted file"))
        || text.startsWith(QLatin1String("rename"))) {
        format.setForeground(QColor(0x55, 0x55, 0x55));
        format.setFontWeight(QFont::Bold);
    } else if (text.startsWith(QLatin1Char('@'))) {
        format.setForeground(QColor(0x35, 0x7a, 0xbd));
    } else if (text.startsWith(QLatin1Char('+')) && !text.startsWith(QLatin1String("+++"))) {
        format.setForeground(QColor(0x1a, 0x7a, 0x3a));
        format.setBackground(QColor(0xe8, 0xf8, 0xec));
    } else if (text.startsWith(QLatin1Char('-')) && !text.startsWith(QLatin1String("---"))) {
        format.setForeground(QColor(0xb3, 0x1d, 0x28));
        format.setBackground(QColor(0xfd, 0xed, 0xed));
    } else {
        return;
    }

    setFormat(0, text.length(), format);
}
