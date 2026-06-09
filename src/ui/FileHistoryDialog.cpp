#include "ui/FileHistoryDialog.h"

#include "ui/DiffDisplay.h"
#include "ui/TopLevelDialogUtils.h"

#include "git/GitService.h"
#include "ui/DialogTitleBar.h"
#include "ui/DiffViewerWidget.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QShortcut>
#include <QSizeGrip>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

QString fileContentText(const WorkingFileContent &content, const QString &missingLabel,
                        const QString &binaryLabel)
{
    if (content.binary) {
        return binaryLabel;
    }
    if (content.missing) {
        return missingLabel;
    }
    if (content.ok) {
        return content.content;
    }
    return content.error;
}

} // namespace

FileHistoryDialog::FileHistoryDialog(GitService *git, const QString &repoPath,
                                     const QString &filePath, QWidget *parent)
    : QWidget(parent), m_git(git), m_repoPath(repoPath), m_filePath(filePath)
{
    const QString title = tr("File history: %1").arg(filePath);
    setWindowTitle(title);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(480, 320);
    resize(1200, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(new DialogTitleBar(title, this));

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    auto *summary = new QLabel(
        tr("Commits that changed <b>%1</b>. Select a commit to view its diff.")
            .arg(filePath.toHtmlEscaped()),
        content);
    summary->setWordWrap(true);
    summary->setTextFormat(Qt::RichText);
    contentLayout->addWidget(summary);

    auto *splitter = new QSplitter(Qt::Vertical, content);

    m_commitList = new QListWidget(splitter);
    m_commitList->setMinimumHeight(160);
    connect(m_commitList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) {
            return;
        }
        QListWidgetItem *item = m_commitList->item(row);
        if (!item) {
            return;
        }
        const QString hash = item->data(Qt::UserRole).toString();
        if (!hash.isEmpty()) {
            loadDiffForCommit(hash);
        }
    });

    m_diffViewer = new DiffViewerWidget(splitter);
    m_diffViewer->setSourceFilePath(filePath);

    splitter->addWidget(m_commitList);
    splitter->addWidget(m_diffViewer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({220, 500});

    contentLayout->addWidget(splitter, 1);

    auto *gripRow = new QHBoxLayout();
    gripRow->addStretch();
    gripRow->addWidget(new QSizeGrip(content));
    contentLayout->addLayout(gripRow);

    layout->addWidget(content, 1);

    auto *closeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    closeShortcut->setContext(Qt::WindowShortcut);
    connect(closeShortcut, &QShortcut::activated, this, &QWidget::close);
}

void FileHistoryDialog::populateCommitList(const std::vector<Commit> &commits)
{
    m_commitList->clear();
    for (const Commit &commit : commits) {
        const QString label =
            tr("%1 — %2 — %3")
                .arg(commit.hash.left(8), commit.date, commit.subject);
        auto *item = new QListWidgetItem(label, m_commitList);
        item->setData(Qt::UserRole, commit.hash);
        item->setToolTip(
            tr("%1\n%2\n%3").arg(commit.hash, commit.author, commit.subject));
    }

    if (commits.empty()) {
        m_commitList->addItem(tr("(no commits found)"));
        m_diffViewer->clear();
        m_diffViewer->setDiff(tr("No commits found that changed this file."));
        return;
    }

    m_commitList->setCurrentRow(0);
}

void FileHistoryDialog::loadDiffForCommit(const QString &hash)
{
    if (!m_git || m_repoPath.isEmpty() || hash.isEmpty()) {
        return;
    }

    const QString diff = m_git->commitFileDiff(m_repoPath, hash, m_filePath);
    if (m_git->lastDiffWasBinary()) {
        m_diffViewer->clear();
        m_diffViewer->setDiff(binaryDiffUserMessage(this));
        return;
    }
    if (diff.isEmpty() && !m_git->lastError().isEmpty()) {
        m_diffViewer->clear();
        m_diffViewer->setDiff(m_git->lastError());
        return;
    }
    if (diff.isEmpty()) {
        m_diffViewer->clear();
        m_diffViewer->setDiff(tr("No diff output for this file in this commit."));
        return;
    }

    const WorkingFileContent before =
        m_git->commitFileContent(m_repoPath, hash, m_filePath, WorkingFileSide::Before);
    const WorkingFileContent after =
        m_git->commitFileContent(m_repoPath, hash, m_filePath, WorkingFileSide::After);

    m_diffViewer->setDiff(diff);
    m_diffViewer->setSources(
        fileContentText(before, tr("(file did not exist in parent commit)"),
                          tr("(binary file — cannot display as text)")),
        fileContentText(after, tr("(file removed in this commit)"),
                          tr("(binary file — cannot display as text)")),
        tr("Before (parent)"), tr("After (commit)"));
}

void FileHistoryDialog::open(QWidget *parent, GitService *git, const QString &repoPath,
                             const QString &filePath)
{
    if (!git || repoPath.isEmpty() || filePath.isEmpty()) {
        return;
    }

    const std::vector<Commit> commits = git->logFileHistory(repoPath, filePath);
    if (commits.empty() && !git->lastError().isEmpty()) {
        QMessageBox::critical(parent, tr("File history"),
                              tr("Failed to load file history:\n%1").arg(git->lastError()));
        return;
    }

    auto *dialog = new FileHistoryDialog(git, repoPath, filePath, nullptr);
    dialog->populateCommitList(commits);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connectRestoreHiddenOwner(dialog, parent);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
