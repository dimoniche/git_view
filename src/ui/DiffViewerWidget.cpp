#include "ui/DiffViewerWidget.h"

#include "ui/DiffDisplay.h"
#include "ui/DiffHighlighter.h"
#include "ui/SourceChangeHighlighter.h"
#include "ui/SourceCodeView.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPoint>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFormat>
#include <QVBoxLayout>

namespace {

QFont diffMonospaceFont(const QFont &base)
{
    QFont font = base;
    font.setStyleHint(QFont::Monospace);
    font.setFamily(QStringLiteral("Menlo"));
#if defined(Q_OS_WIN)
    font.setFamily(QStringLiteral("Consolas"));
#elif defined(Q_OS_LINUX)
    font.setFamily(QStringLiteral("Monospace"));
#endif
    return font;
}

QWidget *labeledEditor(const QString &caption, QLabel **labelOut, SourceCodeView **editorOut,
                       QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *label = new QLabel(caption, container);
    label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(label);
    *labelOut = label;

    auto *editor = new SourceCodeView(container);
    layout->addWidget(editor, 1);
    *editorOut = editor;
    return container;
}

} // namespace

DiffViewerWidget::DiffViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_rootSplitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(m_rootSplitter);

    m_diffView = new QPlainTextEdit(m_rootSplitter);
    configureEditor(m_diffView);
    new DiffHighlighter(m_diffView->document(), m_diffView);
    connect(m_diffView, &QPlainTextEdit::cursorPositionChanged, this,
            &DiffViewerWidget::onDiffCursorChanged);

    m_sourceSplitter = new QSplitter(Qt::Horizontal, m_rootSplitter);
    labeledEditor(tr("Before"), &m_beforeLabel, &m_beforeView, m_sourceSplitter);
    labeledEditor(tr("After"), &m_afterLabel, &m_afterView, m_sourceSplitter);
    configureEditor(m_beforeView);
    configureEditor(m_afterView);
    m_beforeHighlighter = new SourceChangeHighlighter(SourceChangeHighlighter::Kind::Removed,
                                                      m_beforeView, m_beforeView->document());
    m_afterHighlighter = new SourceChangeHighlighter(SourceChangeHighlighter::Kind::Added,
                                                     m_afterView, m_afterView->document());

    m_beforeView->setScrollPartner(m_afterView);
    m_afterView->setScrollPartner(m_beforeView);

    connect(m_beforeView, &QPlainTextEdit::cursorPositionChanged, this,
            &DiffViewerWidget::onSourceCursorChanged);
    connect(m_afterView, &QPlainTextEdit::cursorPositionChanged, this,
            &DiffViewerWidget::onSourceCursorChanged);

    m_rootSplitter->addWidget(m_diffView);
    m_rootSplitter->addWidget(m_sourceSplitter);
    m_rootSplitter->setStretchFactor(0, 1);
    m_rootSplitter->setStretchFactor(1, 1);
    m_rootSplitter->setSizes({480, 480});

    m_diffView->viewport()->installEventFilter(this);
    m_beforeView->viewport()->installEventFilter(this);
    m_afterView->viewport()->installEventFilter(this);

    m_sourceSplitter->setStretchFactor(0, 1);
    m_sourceSplitter->setStretchFactor(1, 1);
    m_sourceSplitter->setSizes({360, 360});
    m_sourceSplitter->setVisible(false);
}

bool DiffViewerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton
            && (watched == m_diffView->viewport() || watched == m_beforeView->viewport()
                || watched == m_afterView->viewport())) {
            m_pendingClickViewport = watched;
            m_pendingClickPos = mouseEvent->pos();
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && watched == m_pendingClickViewport) {
            m_pendingClickViewport = nullptr;
            const int dragDistance = (mouseEvent->pos() - m_pendingClickPos).manhattanLength();
            if (dragDistance <= QApplication::startDragDistance()) {
                if (watched == m_diffView->viewport() && !m_diffView->textCursor().hasSelection()) {
                    handleDiffClick(mouseEvent->pos());
                } else if (watched == m_beforeView->viewport()
                           && !m_beforeView->textCursor().hasSelection()) {
                    handleSourceClick(m_beforeView, mouseEvent->pos());
                } else if (watched == m_afterView->viewport()
                           && !m_afterView->textCursor().hasSelection()) {
                    handleSourceClick(m_afterView, mouseEvent->pos());
                }
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void DiffViewerWidget::handleDiffClick(const QPoint &viewportPos)
{
    if (m_lineMap.isEmpty() || !m_sourceSplitter->isVisible()) {
        return;
    }

    const QTextCursor cursor = m_diffView->cursorForPosition(viewportPos);
    if (cursor.isNull()) {
        return;
    }

    m_diffView->setTextCursor(cursor);
    navigateFromDiffLineIndex(cursor.blockNumber());
}

void DiffViewerWidget::handleSourceClick(SourceCodeView *view, const QPoint &viewportPos)
{
    if (!view || m_syncingSourceNavigation || !m_sourceSplitter->isVisible()) {
        return;
    }

    const QTextCursor cursor = view->cursorForPosition(viewportPos);
    if (cursor.isNull()) {
        return;
    }

    view->setTextCursor(cursor);
    navigateFromSourceDisplayRow(cursor.blockNumber() + 1);
}

void DiffViewerWidget::configureEditor(QPlainTextEdit *editor)
{
    editor->setReadOnly(true);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    editor->setFont(diffMonospaceFont(editor->font()));
    editor->setTabStopDistance(
        QFontMetrics(editor->font()).horizontalAdvance(QLatin1Char(' ')) * 4);
    editor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editor, &QWidget::customContextMenuRequested, editor,
            [editor](const QPoint &pos) {
                if (QMenu *menu = editor->createStandardContextMenu()) {
                    menu->exec(editor->mapToGlobal(pos));
                    delete menu;
                }
            });
}

void DiffViewerWidget::setDiff(const QString &diff)
{
    m_rawDiff = diff;
    updatePresentation();
}

bool DiffViewerWidget::contentUnchanged() const
{
    if (m_rawDiff.isEmpty()
        || DiffParser::shouldSkipExpensiveDiffProcessing(m_rawDiff, m_rawBefore, m_rawAfter)) {
        return false;
    }

    if (!m_rawBefore.isEmpty() && !m_rawAfter.isEmpty()
        && !m_rawBefore.startsWith(QLatin1Char('('))
        && !m_rawAfter.startsWith(QLatin1Char('('))
        && DiffParser::fileContentsEquivalent(m_rawBefore, m_rawAfter)) {
        return true;
    }

    return DiffParser::diffShowsNoContentChange(
        DiffParser::prepareDiffForDisplay(m_rawDiff, m_rawBefore, m_rawAfter));
}

void DiffViewerWidget::updatePresentation()
{
    const QString displayDiff =
        DiffParser::prepareDiffForDisplay(m_rawDiff, m_rawBefore, m_rawAfter);

    if (contentUnchanged()) {
        m_lineMap.clear();
        m_alignedView = {};
        m_diffView->setPlainText(equivalentContentDespiteDiffMessage(this));

        const bool hasBefore = !m_rawBefore.isEmpty();
        const bool hasAfter = !m_rawAfter.isEmpty();
        m_sourceSplitter->setVisible(hasBefore || hasAfter);

        m_beforeView->setPlainText(hasBefore ? m_rawBefore : QString());
        m_afterView->setPlainText(hasAfter ? m_rawAfter : QString());

        if (m_beforeHighlighter) {
            m_beforeHighlighter->setPaddingLines({});
            m_beforeHighlighter->setChangedLines({});
        }
        if (m_afterHighlighter) {
            m_afterHighlighter->setPaddingLines({});
            m_afterHighlighter->setChangedLines({});
        }
        return;
    }

    m_lineMap = DiffParser::buildDiffLineMap(displayDiff);
    m_diffView->setPlainText(displayDiff);
    if (!m_rawBefore.isEmpty() || !m_rawAfter.isEmpty()) {
        applyAlignedSources();
    }
}

void DiffViewerWidget::setSourceFilePath(const QString &path)
{
    m_sourceFilePath = path;
    if (m_beforeHighlighter) {
        m_beforeHighlighter->setFilePath(path);
    }
    if (m_afterHighlighter) {
        m_afterHighlighter->setFilePath(path);
    }
}

void DiffViewerWidget::setSources(const QString &beforeText, const QString &afterText,
                                  const QString &beforeCaption, const QString &afterCaption)
{
    m_rawBefore = beforeText;
    m_rawAfter = afterText;

    const bool hasBefore = !beforeText.isEmpty();
    const bool hasAfter = !afterText.isEmpty();
    m_sourceSplitter->setVisible(hasBefore || hasAfter);

    if (m_beforeLabel) {
        m_beforeLabel->setText(beforeCaption.isEmpty() ? tr("Before") : beforeCaption);
    }
    if (m_afterLabel) {
        m_afterLabel->setText(afterCaption.isEmpty() ? tr("After") : afterCaption);
    }

    updatePresentation();
    onDiffCursorChanged();
}

void DiffViewerWidget::applyAlignedSources()
{
    m_alignedView =
        DiffParser::buildAlignedSideBySideView(m_rawBefore, m_rawAfter, m_lineMap);
    DiffParser::applyDiffHighlightsToAlignedView(&m_alignedView, m_lineMap);

    m_beforeView->setPlainText(m_alignedView.beforeText);
    m_afterView->setPlainText(m_alignedView.afterText);
    applySourceHighlights();
}

void DiffViewerWidget::applySourceHighlights()
{
    if (m_beforeHighlighter) {
        m_beforeHighlighter->setPaddingLines(m_alignedView.beforePaddingRows);
        m_beforeHighlighter->setChangedLines(m_alignedView.beforeChangedRows);
    }
    if (m_afterHighlighter) {
        m_afterHighlighter->setPaddingLines(m_alignedView.afterPaddingRows);
        m_afterHighlighter->setChangedLines(m_alignedView.afterChangedRows);
    }
}

void DiffViewerWidget::clear()
{
    m_lineMap.clear();
    m_alignedView = {};
    m_rawDiff.clear();
    m_rawBefore.clear();
    m_rawAfter.clear();
    m_sourceFilePath.clear();
    m_diffView->clear();
    m_beforeView->clear();
    m_afterView->clear();
    if (m_beforeHighlighter) {
        m_beforeHighlighter->setPaddingLines({});
        m_beforeHighlighter->setChangedLines({});
    }
    if (m_afterHighlighter) {
        m_afterHighlighter->setPaddingLines({});
        m_afterHighlighter->setChangedLines({});
    }
    m_sourceSplitter->setVisible(false);
}

int DiffViewerWidget::displayRowForBeforeSourceLine(int sourceLine) const
{
    return m_alignedView.beforeSourceToDisplay.value(sourceLine, 0);
}

int DiffViewerWidget::displayRowForAfterSourceLine(int sourceLine) const
{
    return m_alignedView.afterSourceToDisplay.value(sourceLine, 0);
}

void DiffViewerWidget::onDiffCursorChanged()
{
    if (m_lineMap.isEmpty() || !m_sourceSplitter->isVisible()
        || m_diffView->textCursor().hasSelection()) {
        return;
    }

    navigateFromDiffLineIndex(m_diffView->textCursor().blockNumber());
}

void DiffViewerWidget::navigateFromDiffLineIndex(int lineIndex)
{
    if (lineIndex < 0 || lineIndex >= m_lineMap.size()) {
        return;
    }

    const DiffParser::DiffLineMap &mapped = m_lineMap.at(lineIndex);

    m_syncingSourceNavigation = true;

    int displayRow = 0;
    if (lineIndex < m_alignedView.diffLineToDisplayRow.size()) {
        displayRow = m_alignedView.diffLineToDisplayRow.at(lineIndex);
    }
    if (displayRow < 1) {
        const int beforeLine = mapped.oldLine > 0 ? mapped.oldLine : -1;
        const int afterLine = mapped.newLine > 0 ? mapped.newLine : -1;
        if (beforeLine > 0) {
            displayRow = displayRowForBeforeSourceLine(beforeLine);
        } else if (afterLine > 0) {
            displayRow = displayRowForAfterSourceLine(afterLine);
        }
    }

    if (displayRow > 0) {
        m_beforeView->revealDisplayLineCentered(displayRow);
        applyDisplayLineSelection(m_beforeView, displayRow);
        applyDisplayLineSelection(m_afterView, displayRow);
    }

    m_syncingSourceNavigation = false;
}

void DiffViewerWidget::onSourceCursorChanged()
{
    if (m_syncingSourceNavigation || !m_sourceSplitter->isVisible()) {
        return;
    }

    auto *source = qobject_cast<SourceCodeView *>(sender());
    if (!source || source->textCursor().hasSelection()) {
        return;
    }

    navigateFromSourceDisplayRow(source->textCursor().blockNumber() + 1);
}

void DiffViewerWidget::navigateFromSourceDisplayRow(int displayRow)
{
    if (displayRow < 1) {
        return;
    }

    m_beforeView->revealDisplayLineCentered(displayRow);
    applyDisplayLineSelection(m_beforeView, displayRow);
    applyDisplayLineSelection(m_afterView, displayRow);
}

void DiffViewerWidget::revealMatchingLines(int beforeLine, int afterLine)
{
    int displayRow = 0;
    SourceCodeView *leader = m_beforeView;

    if (beforeLine > 0) {
        displayRow = displayRowForBeforeSourceLine(beforeLine);
    } else if (afterLine > 0) {
        displayRow = displayRowForAfterSourceLine(afterLine);
        leader = m_afterView;
    }

    if (displayRow < 1) {
        return;
    }

    leader->revealDisplayLineCentered(displayRow);
}

void DiffViewerWidget::applyDisplayLineSelection(QPlainTextEdit *editor, int displayLine)
{
    if (!editor) {
        return;
    }

    if (displayLine < 1) {
        editor->setExtraSelections({});
        return;
    }

    QTextBlock block = editor->document()->findBlockByNumber(displayLine - 1);
    if (!block.isValid()) {
        editor->setExtraSelections({});
        return;
    }

    QTextEdit::ExtraSelection selection;
    QTextCharFormat format;
    format.setBackground(QColor(0x00, 0x6c, 0xc1));
    format.setForeground(Qt::white);
    format.setFontWeight(QFont::Bold);
    format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.format = format;
    selection.cursor = QTextCursor(block);
    selection.cursor.clearSelection();
    editor->setExtraSelections({selection});
}
