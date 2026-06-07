#pragma once

#include <QSet>
#include <QSyntaxHighlighter>

class QTextDocument;

class SourceChangeHighlighter : public QSyntaxHighlighter {
public:
    enum class Kind { Removed, Added };

    SourceChangeHighlighter(Kind kind, QTextDocument *document = nullptr);

    void setChangedLines(const QSet<int> &lineNumbers);
    void setPaddingLines(const QSet<int> &lineNumbers);

protected:
    void highlightBlock(const QString &text) override;

private:
    Kind m_kind;
    QSet<int> m_changedLines;
    QSet<int> m_paddingLines;
};
