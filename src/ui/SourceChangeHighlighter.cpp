#include "ui/SourceChangeHighlighter.h"

#include "ui/CodeSyntax.h"
#include "ui/EditorTheme.h"

#include <QColor>
#include <QFont>
#include <QTextCharFormat>
#include <QTextFormat>
#include <QWidget>

SourceChangeHighlighter::SourceChangeHighlighter(Kind kind, QWidget *editor, QTextDocument *document)
    : QSyntaxHighlighter(document), m_kind(kind), m_editor(editor)
{
    m_keywordFormat.setForeground(QColor(0x00, 0x00, 0xff));
    m_keywordFormat.setFontWeight(QFont::Bold);

    m_stringFormat.setForeground(QColor(0xa3, 0x15, 0x15));

    m_commentFormat.setForeground(QColor(0x00, 0x80, 0x00));
    m_commentFormat.setFontItalic(true);

    m_numberFormat.setForeground(QColor(0x09, 0x86, 0x58));

    m_preprocessorFormat.setForeground(QColor(0x79, 0x5e, 0x26));
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

void SourceChangeHighlighter::setFilePath(const QString &path)
{
    m_language = codeLanguageForPath(path);
    rehighlight();
}

void SourceChangeHighlighter::applySyntaxFormat(int start, int length, const QTextCharFormat &format,
                                                void *userData)
{
    auto *highlighter = static_cast<SourceChangeHighlighter *>(userData);
    QTextCharFormat merged = format;
    if (highlighter->m_applyChangeBackground) {
        merged.setBackground(highlighter->m_changeBackgroundColor);
    }
    highlighter->setFormat(start, length, merged);
}

void SourceChangeHighlighter::highlightBlock(const QString &text)
{
    const int lineNumber = currentBlock().blockNumber() + 1;
    const bool dark = editorUsesDarkTheme(m_editor);

    if (m_paddingLines.contains(lineNumber)) {
        QTextCharFormat format;
        format.setBackground(paddingLineBackground(dark));
        format.setProperty(QTextFormat::FullWidthSelection, true);
        setFormat(0, currentBlock().length(), format);
        return;
    }

    const bool isChanged = m_changedLines.contains(lineNumber);
    m_applyChangeBackground = isChanged;

    QTextCharFormat keywordFormat = m_keywordFormat;
    QTextCharFormat stringFormat = m_stringFormat;
    QTextCharFormat commentFormat = m_commentFormat;
    QTextCharFormat numberFormat = m_numberFormat;
    QTextCharFormat preprocessorFormat = m_preprocessorFormat;

    if (dark && !isChanged) {
        keywordFormat.setForeground(QColor(0x56, 0x9c, 0xd6));
        keywordFormat.setFontWeight(QFont::Bold);
        stringFormat.setForeground(QColor(0xce, 0x91, 0x78));
        commentFormat.setForeground(QColor(0x6a, 0x99, 0x55));
        commentFormat.setFontItalic(true);
        numberFormat.setForeground(QColor(0xb5, 0xce, 0xa8));
        preprocessorFormat.setForeground(QColor(0xc5, 0x86, 0xc0));
    }

    if (isChanged) {
        const ChangeHighlightTheme theme =
            m_kind == Kind::Removed ? removedChangeTheme(dark) : addedChangeTheme(dark);
        m_changeBackgroundColor = theme.lineBackground;

        QTextCharFormat lineBackground;
        lineBackground.setBackground(theme.lineBackground);
        lineBackground.setForeground(theme.defaultText);
        lineBackground.setProperty(QTextFormat::FullWidthSelection, true);
        setFormat(0, text.length(), lineBackground);

        keywordFormat.setForeground(theme.keyword);
        keywordFormat.setFontWeight(QFont::Bold);
        stringFormat.setForeground(theme.string);
        commentFormat.setForeground(theme.comment);
        commentFormat.setFontItalic(true);
        numberFormat.setForeground(theme.number);
        preprocessorFormat.setForeground(theme.preprocessor);
    }

    int nextState = 0;
    highlightCodeSyntax(m_language, text, currentBlockState(), &nextState, keywordFormat,
                        stringFormat, commentFormat, numberFormat, preprocessorFormat,
                        &SourceChangeHighlighter::applySyntaxFormat, this);
    setCurrentBlockState(nextState);
}
