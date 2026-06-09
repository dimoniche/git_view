#include "ui/CommitDetailsDialog.h"

#include "ui/DialogTitleBar.h"
#include "ui/DiffDisplay.h"
#include "ui/DiffViewerDialog.h"
#include "ui/DiffViewerWidget.h"
#include "ui/FileHistoryDialog.h"

#include <QClipboard>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMenu>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QShortcut>
#include <QSizeGrip>
#include <QSplitter>
#include <QTimer>
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

CommitDetailsDialog::CommitDetailsDialog(GitService *git, const QString &repoPath,
                                         const CommitDetails &details, QWidget *parent)
    : QWidget(parent), m_git(git), m_repoPath(repoPath), m_details(details)
{
    const QString title =
        tr("Commit %1 — %2").arg(details.commit.hash.left(8), details.commit.subject);
    setWindowTitle(title);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(640, 400);
    resize(1200, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(new DialogTitleBar(title, this));

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    auto *summary = new QLabel(
        tr("<b>%1</b><br/>%2 · %3<br/><code>%4</code>")
            .arg(details.commit.subject.toHtmlEscaped(),
                 details.commit.author.toHtmlEscaped(),
                 details.commit.date.toHtmlEscaped(),
                 details.commit.hash.toHtmlEscaped()),
        content);
    summary->setWordWrap(true);
    summary->setTextFormat(Qt::RichText);
    contentLayout->addWidget(summary);

    m_splitter = new QSplitter(Qt::Horizontal, content);

    m_filesList = new QListWidget(m_splitter);
    m_filesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_filesList->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_filesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_filesList, &QWidget::customContextMenuRequested, this,
            &CommitDetailsDialog::showFilesContextMenu);
    connect(m_filesList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) {
            return;
        }
        QListWidgetItem *item = m_filesList->item(row);
        if (!item) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            loadDiffForFile(path);
        }
    });
    connect(m_filesList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (!item) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            openDiffInSeparateWindow(path);
        }
    });

    m_diffViewer = new DiffViewerWidget(m_splitter);
    m_diffViewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_splitter->addWidget(m_filesList);
    m_splitter->addWidget(m_diffViewer);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);

    contentLayout->addWidget(m_splitter, 1);

    auto *gripRow = new QHBoxLayout();
    gripRow->addStretch();
    gripRow->addWidget(new QSizeGrip(content));
    contentLayout->addLayout(gripRow);

    layout->addWidget(content, 1);

    auto *closeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    closeShortcut->setContext(Qt::WindowShortcut);
    connect(closeShortcut, &QShortcut::activated, this, &QWidget::close);

    populateFileList();
}

void CommitDetailsDialog::populateFileList()
{
    m_filesList->clear();
    for (const CommitFileChange &change : m_details.files) {
        auto *item = new QListWidgetItem(QStringLiteral("[%1] %2").arg(change.status, change.path),
                                         m_filesList);
        item->setData(Qt::UserRole, change.path);
        item->setToolTip(change.path);
    }

    adjustFileListWidth();

    if (m_details.files.empty()) {
        m_filesList->addItem(tr("(no file changes)"));
        m_diffViewer->clear();
        m_diffViewer->setDiff(tr("This commit has no file changes."));
        return;
    }

    m_filesList->setCurrentRow(0);
    loadDiffForFile(m_details.files.front().path);
}

void CommitDetailsDialog::adjustFileListWidth()
{
    const QFontMetrics metrics(m_filesList->font());
    int contentWidth = metrics.horizontalAdvance(tr("(no file changes)"));
    for (int row = 0; row < m_filesList->count(); ++row) {
        const QListWidgetItem *item = m_filesList->item(row);
        if (!item) {
            continue;
        }
        contentWidth = std::max(contentWidth, metrics.horizontalAdvance(item->text()));
    }

    const int frame = m_filesList->frameWidth() * 2 + 32;
    const int listWidth = contentWidth + frame;
    m_filesList->setFixedWidth(listWidth);

    if (!m_splitter) {
        return;
    }

    QTimer::singleShot(0, this, [this, listWidth]() {
        if (!m_splitter) {
            return;
        }
        const int totalWidth = m_splitter->width();
        const int diffWidth = std::max(totalWidth - listWidth, 320);
        m_splitter->setSizes({listWidth, diffWidth});
    });
}

void CommitDetailsDialog::loadDiffForFile(const QString &path)
{
    if (!m_git || m_repoPath.isEmpty() || m_details.commit.hash.isEmpty() || path.isEmpty()) {
        return;
    }

    m_diffViewer->setSourceFilePath(path);

    const QString diff = m_git->commitFileDiff(m_repoPath, m_details.commit.hash, path);
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
        m_diffViewer->setDiff(tr("No diff output for this file."));
        return;
    }

    const DiffViewerSources sources = buildSourcesForFile(path);
    m_diffViewer->setDiff(diff);
    m_diffViewer->setSources(sources.before, sources.after, sources.beforeCaption,
                             sources.afterCaption);
}

DiffViewerSources CommitDetailsDialog::buildSourcesForFile(const QString &path) const
{
    DiffViewerSources sources;
    sources.beforeCaption = tr("Before (parent)");
    sources.afterCaption = tr("After (commit)");

    if (!m_git || m_repoPath.isEmpty() || m_details.commit.hash.isEmpty()) {
        return sources;
    }

    const WorkingFileContent before =
        m_git->commitFileContent(m_repoPath, m_details.commit.hash, path, WorkingFileSide::Before);
    const WorkingFileContent after =
        m_git->commitFileContent(m_repoPath, m_details.commit.hash, path, WorkingFileSide::After);

    sources.before = fileContentText(
        before, tr("(file did not exist in parent commit)"),
        tr("(binary file — cannot display as text)"));
    sources.after = fileContentText(
        after, tr("(file removed in this commit)"),
        tr("(binary file — cannot display as text)"));

    return sources;
}

void CommitDetailsDialog::showFilesContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_filesList->itemAt(pos);
    if (!item) {
        return;
    }

    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }

    m_filesList->setCurrentRow(m_filesList->row(item));

    QMenu menu(this);
    menu.addAction(tr("Show full file history…"), this,
                   [this, path]() { openFileHistoryInSeparateWindow(path); });
    menu.addSeparator();
    menu.addAction(tr("Copy path"), this, [path]() {
        if (QClipboard *clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(path);
        }
    });
    menu.exec(m_filesList->mapToGlobal(pos));
}

void CommitDetailsDialog::openFileHistoryInSeparateWindow(const QString &path)
{
    FileHistoryDialog::open(this, m_git, m_repoPath, path);
}

void CommitDetailsDialog::openDiffInSeparateWindow(const QString &path)
{
    if (!m_git || m_repoPath.isEmpty() || m_details.commit.hash.isEmpty() || path.isEmpty()) {
        return;
    }

    const QString title = tr("Diff: %1").arg(path);
    const QString diff = m_git->commitFileDiff(m_repoPath, m_details.commit.hash, path);
    if (m_git->lastDiffWasBinary()) {
        DiffViewerDialog::showDiff(this, title, binaryDiffUserMessage(this));
        return;
    }
    if (diff.isEmpty() && !m_git->lastError().isEmpty()) {
        DiffViewerDialog::showDiff(this, title, m_git->lastError());
        return;
    }
    if (diff.isEmpty()) {
        DiffViewerDialog::showDiff(this, title, tr("No diff output for this file."));
        return;
    }

    DiffViewerDialog::showDiff(this, title, diff, buildSourcesForFile(path), path);
}

void CommitDetailsDialog::open(QWidget *parent, GitService *git, const QString &repoPath,
                               const QString &commitHash)
{
    if (!git || repoPath.isEmpty() || commitHash.isEmpty()) {
        return;
    }

    const CommitDetails details = git->commitDetails(repoPath, commitHash);
    if (details.commit.hash.isEmpty() && !git->lastError().isEmpty()) {
        QMessageBox::critical(parent, QObject::tr("Commit details"),
                              QObject::tr("Failed to load commit details:\n%1")
                                  .arg(git->lastError()));
        return;
    }

    auto *dialog = new CommitDetailsDialog(git, repoPath, details, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
