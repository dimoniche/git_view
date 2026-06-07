#include "ui/SourceChangeHighlighter.h"

#include <QColor>
#include <QTextCharFormat>
#include <QTextFormat>

SourceChangeHighlighter::SourceChangeHighlighter(Kind kind, QTextDocument *document)
    : QSyntaxHighlighter(document)
    , m_kind(kind)
{
}

void SourceChangeHighlighter::setChangedLines(const QSet<int> &lineNumbers)
{
    m_changedLines = lineNumbers;
    rehighlight();
}

void SourceChangeHighlighter::setPaddingLines(const QSet<int> &lineNumbers)
{
    m_paddingLines = lineNumbers;
    rehighlight();
}

void SourceChangeHighlighter::highlightBlock(const QString &text)
{
    Q_UNUSED(text);

    const int lineNumber = currentBlock().blockNumber() + 1;

    if (m_paddingLines.contains(lineNumber)) {
        QTextCharFormat format;
        format.setBackground(QColor(0xe4, 0xe4, 0xe4));
        format.setProperty(QTextFormat::FullWidthSelection, true);
        setFormat(0, currentBlock().length(), format);
        return;
    }

    if (!m_changedLines.contains(lineNumber)) {
        return;
    }

    QTextCharFormat format;
    if (m_kind == Kind::Removed) {
        format.setForeground(QColor(0xb3, 0x1d, 0x28));
        format.setBackground(QColor(0xfd, 0xed, 0xed));
    } else {
        format.setForeground(QColor(0x1a, 0x7a, 0x3a));
        format.setBackground(QColor(0xe8, 0xf8, 0xec));
    }

    setFormat(0, currentBlock().length(), format);
}
