#pragma once

#include <QSyntaxHighlighter>

class QTextDocument;

class DiffHighlighter : public QSyntaxHighlighter {
public:
    explicit DiffHighlighter(QTextDocument *document = nullptr);

protected:
    void highlightBlock(const QString &text) override;
};
