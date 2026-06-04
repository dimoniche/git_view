#include "ui/CommitDetailsPanel.h"

#include "git/GitService.h"
#include "ui/DiffHighlighter.h"

#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QVBoxLayout>

CommitDetailsPanel::CommitDetailsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summaryLabel);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    auto *filesWidget = new QWidget(splitter);
    auto *filesLayout = new QVBoxLayout(filesWidget);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->addWidget(new QLabel(tr("Changed files"), filesWidget));

    m_filesList = new QListWidget(filesWidget);
    m_filesList->setMinimumHeight(80);
    connect(m_filesList, &QListWidget::currentRowChanged, this,
            &CommitDetailsPanel::onFileSelectionChanged);
    filesLayout->addWidget(m_filesList);

    splitter->addWidget(filesWidget);

    auto *diffWidget = new QWidget(splitter);
    auto *diffLayout = new QVBoxLayout(diffWidget);
    diffLayout->setContentsMargins(0, 0, 0, 0);

    m_diffTitle = new QLabel(tr("Diff"), diffWidget);
    diffLayout->addWidget(m_diffTitle);

    m_diffView = new QPlainTextEdit(diffWidget);
    m_diffView->setReadOnly(true);
    m_diffView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diffView->setTabStopDistance(QFontMetrics(m_diffView->font()).horizontalAdvance(QLatin1Char(' ')) * 4);

    QFont diffFont = m_diffView->font();
    diffFont.setStyleHint(QFont::Monospace);
    diffFont.setFamily(QStringLiteral("Menlo"));
#if defined(Q_OS_WIN)
    diffFont.setFamily(QStringLiteral("Consolas"));
#elif defined(Q_OS_LINUX)
    diffFont.setFamily(QStringLiteral("Monospace"));
#endif
    m_diffView->setFont(diffFont);

    new DiffHighlighter(m_diffView->document());
    diffLayout->addWidget(m_diffView);

    splitter->addWidget(diffWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({140, 360});

    layout->addWidget(splitter, 1);

    clear();
}

void CommitDetailsPanel::setRepoContext(const QString &repoPath, GitService *git)
{
    m_repoPath = repoPath;
    m_git = git;
}

void CommitDetailsPanel::showDetails(const CommitDetails &details)
{
    m_details = details;
    m_commitHash = details.commit.hash;

    m_summaryLabel->setText(
        tr("<b>%1</b><br/>%2<br/>%3<br/><code>%4</code>")
            .arg(details.commit.subject.toHtmlEscaped(),
                 details.commit.author.toHtmlEscaped(),
                 details.commit.date.toHtmlEscaped(),
                 details.commit.hash.toHtmlEscaped()));

    m_filesList->clear();
    for (const CommitFileChange &change : details.files) {
        auto *item = new QListWidgetItem(QStringLiteral("[%1] %2").arg(change.status, change.path),
                                         m_filesList);
        item->setData(Qt::UserRole, change.path);
        item->setToolTip(change.path);
    }

    if (details.files.empty()) {
        m_filesList->addItem(tr("(no file changes)"));
        m_filesList->setCurrentRow(-1);
        showDiffText({}, {});
        return;
    }

    m_filesList->setCurrentRow(0);
}

void CommitDetailsPanel::clear()
{
    m_commitHash.clear();
    m_details = {};
    m_summaryLabel->setText(tr("Select a commit"));
    m_filesList->clear();
    m_filesList->setCurrentRow(-1);
    showDiffText({}, tr("Diff"));
}

void CommitDetailsPanel::onFileSelectionChanged()
{
    loadDiffForCurrentFile();
}

void CommitDetailsPanel::loadDiffForCurrentFile()
{
    if (!m_git || m_repoPath.isEmpty() || m_commitHash.isEmpty()) {
        showDiffText(tr("Open a repository to view diffs."), tr("Diff"));
        return;
    }

    const QListWidgetItem *item = m_filesList->currentItem();
    if (!item) {
        showDiffText({}, tr("Diff"));
        return;
    }

    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        showDiffText({}, tr("Diff"));
        return;
    }

    m_diffTitle->setText(tr("Diff: %1").arg(path));

    const QString diff = m_git->commitFileDiff(m_repoPath, m_commitHash, path);
    if (diff.isEmpty() && !m_git->lastError().isEmpty()) {
        showDiffText(m_git->lastError(), m_diffTitle->text());
        return;
    }

    if (diff.isEmpty()) {
        showDiffText(tr("No diff output for this file."), m_diffTitle->text());
        return;
    }

    showDiffText(diff, m_diffTitle->text());
}

void CommitDetailsPanel::showDiffText(const QString &text, const QString &title)
{
    if (!title.isEmpty()) {
        m_diffTitle->setText(title);
    }
    m_diffView->setPlainText(text);
}
