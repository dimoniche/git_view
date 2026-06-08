#pragma once

#include "git/DiffParser.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QPoint;

class QObject;
class QLabel;
class QPlainTextEdit;
class QSplitter;
class SourceChangeHighlighter;
class SourceCodeView;

class DiffViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit DiffViewerWidget(QWidget *parent = nullptr);

    void setDiff(const QString &diff);
    void setSources(const QString &beforeText, const QString &afterText,
                    const QString &beforeCaption = QString(),
                    const QString &afterCaption = QString());
    void clear();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onDiffCursorChanged();
    void onSourceCursorChanged();

private:
    void configureEditor(QPlainTextEdit *editor);
    void applyAlignedSources();
    void applySourceHighlights();
    void applyDisplayLineSelection(QPlainTextEdit *editor, int displayLine);
    void handleDiffClick(const QPoint &viewportPos);
    void handleSourceClick(SourceCodeView *view, const QPoint &viewportPos);
    void navigateFromDiffLineIndex(int lineIndex);
    void navigateFromSourceDisplayRow(int displayRow);
    void revealMatchingLines(int beforeLine, int afterLine);
    int displayRowForBeforeSourceLine(int sourceLine) const;
    int displayRowForAfterSourceLine(int sourceLine) const;

    QSplitter *m_rootSplitter = nullptr;
    QPlainTextEdit *m_diffView = nullptr;
    QSplitter *m_sourceSplitter = nullptr;
    QLabel *m_beforeLabel = nullptr;
    QLabel *m_afterLabel = nullptr;
    SourceCodeView *m_beforeView = nullptr;
    SourceCodeView *m_afterView = nullptr;
    SourceChangeHighlighter *m_beforeHighlighter = nullptr;
    SourceChangeHighlighter *m_afterHighlighter = nullptr;
    QVector<DiffParser::DiffLineMap> m_lineMap;
    DiffParser::AlignedSideBySideView m_alignedView;
    QString m_rawBefore;
    QString m_rawAfter;
    bool m_syncingSourceNavigation = false;
    QObject *m_pendingClickViewport = nullptr;
    QPoint m_pendingClickPos;
};
