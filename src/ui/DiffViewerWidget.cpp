#include "ui/DiffViewerWidget.h"

#include "ui/DiffHighlighter.h"
#include "ui/SourceChangeHighlighter.h"
#include "ui/SourceCodeView.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QPlainTextEdit>
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
    new DiffHighlighter(m_diffView->document());
    connect(m_diffView, &QPlainTextEdit::cursorPositionChanged, this,
            &DiffViewerWidget::onDiffCursorChanged);

    m_sourceSplitter = new QSplitter(Qt::Horizontal, m_rootSplitter);
    labeledEditor(tr("Before"), &m_beforeLabel, &m_beforeView, m_sourceSplitter);
    labeledEditor(tr("After"), &m_afterLabel, &m_afterView, m_sourceSplitter);
    configureEditor(m_beforeView);
    configureEditor(m_afterView);
    m_beforeHighlighter =
        new SourceChangeHighlighter(SourceChangeHighlighter::Kind::Removed, m_beforeView->document());
    m_afterHighlighter =
        new SourceChangeHighlighter(SourceChangeHighlighter::Kind::Added, m_afterView->document());

    m_beforeView->setScrollPartner(m_afterView);
    m_afterView->setScrollPartner(m_beforeView);

    m_rootSplitter->addWidget(m_diffView);
    m_rootSplitter->addWidget(m_sourceSplitter);
    m_rootSplitter->setStretchFactor(0, 1);
    m_rootSplitter->setStretchFactor(1, 1);
    m_rootSplitter->setSizes({480, 480});

    m_sourceSplitter->setStretchFactor(0, 1);
    m_sourceSplitter->setStretchFactor(1, 1);
    m_sourceSplitter->setSizes({360, 360});
    m_sourceSplitter->setVisible(false);
}

void DiffViewerWidget::configureEditor(QPlainTextEdit *editor)
{
    editor->setReadOnly(true);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setFont(diffMonospaceFont(editor->font()));
    editor->setTabStopDistance(
        QFontMetrics(editor->font()).horizontalAdvance(QLatin1Char(' ')) * 4);
}

void DiffViewerWidget::setDiff(const QString &diff)
{
    m_lineMap = DiffParser::buildDiffLineMap(diff);
    m_diffView->setPlainText(diff);
    if (!m_rawBefore.isEmpty() || !m_rawAfter.isEmpty()) {
        applyAlignedSources();
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

    applyAlignedSources();
    onDiffCursorChanged();
}

void DiffViewerWidget::applyAlignedSources()
{
    m_alignedView = DiffParser::buildAlignedSideBySideView(m_rawBefore, m_rawAfter);
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
    m_rawBefore.clear();
    m_rawAfter.clear();
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
    if (m_lineMap.isEmpty() || !m_sourceSplitter->isVisible()) {
        return;
    }

    const int lineIndex = m_diffView->textCursor().blockNumber();
    if (lineIndex < 0 || lineIndex >= m_lineMap.size()) {
        return;
    }

    const DiffParser::DiffLineMap &mapped = m_lineMap.at(lineIndex);
    const int beforeLine = mapped.oldLine > 0 ? mapped.oldLine : -1;
    const int afterLine = mapped.newLine > 0 ? mapped.newLine : -1;

    revealMatchingLines(beforeLine, afterLine);

    const int displayRow = beforeLine > 0 ? displayRowForBeforeSourceLine(beforeLine)
                                          : displayRowForAfterSourceLine(afterLine);
    applyDisplayLineSelection(m_beforeView, displayRow);
    applyDisplayLineSelection(m_afterView, displayRow);
}

void DiffViewerWidget::revealMatchingLines(int beforeLine, int afterLine)
{
    int displayRow = 0;
    if (beforeLine > 0) {
        displayRow = displayRowForBeforeSourceLine(beforeLine);
    } else if (afterLine > 0) {
        displayRow = displayRowForAfterSourceLine(afterLine);
    }

    if (displayRow < 1) {
        return;
    }

    m_beforeView->revealDisplayLineCentered(displayRow);
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
