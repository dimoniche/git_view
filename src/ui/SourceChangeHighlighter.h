#pragma once

#include "ui/CodeSyntax.h"

#include <QColor>
#include <QSet>
#include <QSyntaxHighlighter>
#include <QString>

class QTextDocument;
class QTextCharFormat;
class QWidget;

class SourceChangeHighlighter : public QSyntaxHighlighter {
public:
    enum class Kind { Removed, Added };

    SourceChangeHighlighter(Kind kind, QWidget *editor, QTextDocument *document = nullptr);

    void setChangedLines(const QSet<int> &lineNumbers);
    void setPaddingLines(const QSet<int> &lineNumbers);
    void setFilePath(const QString &path);

protected:
    void highlightBlock(const QString &text) override;

private:
    static void applySyntaxFormat(int start, int length, const QTextCharFormat &format, void *userData);

    Kind m_kind;
    QWidget *m_editor = nullptr;
    CodeLanguage m_language = CodeLanguage::Generic;
    QSet<int> m_changedLines;
    QSet<int> m_paddingLines;
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_preprocessorFormat;

    bool m_applyChangeBackground = false;
    QColor m_changeBackgroundColor;
};
