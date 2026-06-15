#pragma once

#include <QSyntaxHighlighter>

class QTextDocument;
class QWidget;

class DiffHighlighter : public QSyntaxHighlighter {
public:
    explicit DiffHighlighter(QTextDocument *document, QWidget *editor = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    QWidget *m_editor = nullptr;
};
