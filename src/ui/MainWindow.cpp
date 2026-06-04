#include "ui/MainWindow.h"

#include "ui/CommitDetailsPanel.h"
#include "ui/CommitHistoryView.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr int kInitialLogLimit = 500;
constexpr int kLogPageSize = 500;
constexpr int kDockMinWidth = 200;

QDockWidget *createDock(const QString &title, const QString &objectName, QWidget *content,
                        QMainWindow *window)
{
    auto *dock = new QDockWidget(title, window);
    dock->setObjectName(objectName);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable
                      | QDockWidget::DockWidgetFloatable);
    dock->setWidget(content);
    dock->setMinimumWidth(kDockMinWidth);
    return dock;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_logLimit(kInitialLogLimit)
{
    setupUi();
    loadRecentRepos();
    setStatusMessage(tr("Open a repository to begin"));
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("git_view"));
    resize(1200, 800);

    auto *openAction = new QAction(tr("&Open repository…"), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openRepository);

    auto *refreshAction = new QAction(tr("&Refresh"), this);
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshRepository);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(refreshAction);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    setDockNestingEnabled(true);

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(6, 6, 6, 6);

    m_repoLabel = new QLabel(tr("No repository"), central);
    m_repoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rootLayout->addWidget(m_repoLabel);

    m_historyScroll = new QScrollArea(central);
    m_historyScroll->setWidgetResizable(true);
    m_historyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyScroll->setFrameShape(QFrame::StyledPanel);

    m_historyView = new CommitHistoryView;
    connect(m_historyView, &CommitHistoryView::commitSelected,
            this, &MainWindow::onCommitSelected);
    m_historyScroll->setWidget(m_historyView);
    rootLayout->addWidget(m_historyScroll, 1);

    m_loadMoreButton = new QPushButton(tr("Load more commits…"), central);
    m_loadMoreButton->setEnabled(false);
    connect(m_loadMoreButton, &QPushButton::clicked, this, &MainWindow::loadMoreCommits);
    rootLayout->addWidget(m_loadMoreButton);

    m_statusLabel = new QLabel(central);
    rootLayout->addWidget(m_statusLabel);

    setCentralWidget(central);

    auto *branchPanel = new QWidget(this);
    auto *branchLayout = new QVBoxLayout(branchPanel);
    branchLayout->setContentsMargins(4, 4, 4, 4);
    m_branchList = new QListWidget(branchPanel);
    connect(m_branchList, &QListWidget::currentRowChanged,
            this, &MainWindow::onBranchSelected);
    branchLayout->addWidget(m_branchList, 1);

    m_mergeButton = new QPushButton(tr("Merge into current…"), branchPanel);
    m_mergeButton->setEnabled(false);
    connect(m_mergeButton, &QPushButton::clicked, this, &MainWindow::mergeSelectedBranch);
    branchLayout->addWidget(m_mergeButton);

    m_branchesDock =
        createDock(tr("Branches"), QStringLiteral("BranchesDock"), branchPanel, this);
    addDockWidget(Qt::LeftDockWidgetArea, m_branchesDock);

    m_detailsPanel = new CommitDetailsPanel(this);
    m_detailsDock = createDock(tr("Commit details"), QStringLiteral("DetailsDock"),
                               m_detailsPanel, this);
    addDockWidget(Qt::RightDockWidgetArea, m_detailsDock);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    QAction *branchesAction = m_branchesDock->toggleViewAction();
    branchesAction->setText(tr("Branches panel"));
    branchesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    viewMenu->addAction(branchesAction);

    QAction *detailsAction = m_detailsDock->toggleViewAction();
    detailsAction->setText(tr("Commit details panel"));
    detailsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    viewMenu->addAction(detailsAction);

    viewMenu->addSeparator();
    viewMenu->addAction(tr("Reset layout"), this, [this]() {
        QSettings settings;
        settings.remove(QStringLiteral("windowGeometry"));
        settings.remove(QStringLiteral("windowState"));
        resize(1200, 800);
        m_branchesDock->show();
        m_detailsDock->show();
        addDockWidget(Qt::LeftDockWidgetArea, m_branchesDock);
        addDockWidget(Qt::RightDockWidgetArea, m_detailsDock);
        resizeDocks({m_branchesDock}, {240}, Qt::Horizontal);
        resizeDocks({m_detailsDock}, {300}, Qt::Horizontal);
    });

    restoreWindowLayout();
    if (QSettings().value(QStringLiteral("windowState")).toByteArray().isEmpty()) {
        resizeDocks({m_branchesDock}, {240}, Qt::Horizontal);
        resizeDocks({m_detailsDock}, {300}, Qt::Horizontal);
    }
}

void MainWindow::restoreWindowLayout()
{
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("windowGeometry")).toByteArray();
    const QByteArray state = settings.value(QStringLiteral("windowState")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (!state.isEmpty()) {
        restoreState(state);
    }
}

void MainWindow::saveWindowLayout()
{
    QSettings settings;
    settings.setValue(QStringLiteral("windowGeometry"), saveGeometry());
    settings.setValue(QStringLiteral("windowState"), saveState());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowLayout();
    QMainWindow::closeEvent(event);
}

void MainWindow::openRepository()
{
    const QString startDir = m_repo.isValid() ? m_repo.path() : QDir::homePath();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Open Git repository"), startDir);
    if (dir.isEmpty()) {
        return;
    }
    setRepository(dir);
}

void MainWindow::refreshRepository()
{
    if (!m_repo.isValid()) {
        return;
    }
    reloadBranches();
    reloadLog(m_branchFilter);
}

void MainWindow::setRepository(const QString &path)
{
    const QString topLevel = m_git.discoverGitDir(path);
    if (topLevel.isEmpty()) {
        QMessageBox::warning(this, tr("Open repository"),
                             tr("Could not open repository:\n%1").arg(m_git.lastError()));
        return;
    }

    m_repo.setPath(topLevel);
    m_logLimit = kInitialLogLimit;
    m_branchFilter.clear();
    saveRecentRepo(topLevel);

    const QString branch = m_git.currentBranch(topLevel);
    const bool dirty = m_git.hasUncommittedChanges(topLevel);
    m_repoLabel->setText(tr("Repository: %1 — branch: %2%3")
                             .arg(topLevel, branch, dirty ? tr(" (dirty)") : QString()));

    m_branchList->clearSelection();
    reloadBranches();
    reloadLog();

    m_mergeButton->setEnabled(true);
    m_loadMoreButton->setEnabled(true);
    setStatusMessage(tr("Loaded %1").arg(topLevel));
}

void MainWindow::reloadBranches()
{
    m_branchList->clear();
    m_branches = m_git.branches(m_repo.path());
    if (m_branches.empty() && !m_git.lastError().isEmpty()) {
        setStatusMessage(m_git.lastError());
        return;
    }

    for (const Branch &branch : m_branches) {
        QString label = branch.name;
        if (branch.isCurrent) {
            label = QStringLiteral("* %1").arg(label);
        }
        if (branch.isRemote) {
            label += QStringLiteral(" [remote]");
        }
        auto *item = new QListWidgetItem(label, m_branchList);
        item->setData(Qt::UserRole, branch.name);
    }
}

void MainWindow::reloadLog(const QString &branchFilter)
{
    m_branchFilter = branchFilter;

    std::vector<Commit> commits;
    if (branchFilter.isEmpty()) {
        commits = m_git.logAll(m_repo.path(), m_logLimit);
    } else {
        commits = m_git.logBranch(m_repo.path(), branchFilter, m_logLimit);
    }

    if (commits.empty() && !m_git.lastError().isEmpty()) {
        m_historyView->setCommits({});
        m_detailsPanel->clear();
        setStatusMessage(m_git.lastError());
        return;
    }

    m_historyView->setCommits(commits);
    m_loadMoreButton->setEnabled(static_cast<int>(commits.size()) >= m_logLimit);

    setStatusMessage(tr("%1 commits shown (limit %2)")
                         .arg(commits.size())
                         .arg(m_logLimit));

    if (!commits.empty()) {
        showCommitDetails(commits.front().hash);
    } else {
        m_detailsPanel->clear();
    }
}

void MainWindow::loadMoreCommits()
{
    m_logLimit += kLogPageSize;
    reloadLog(m_branchFilter);
}

void MainWindow::onCommitSelected(const QString &hash)
{
    showCommitDetails(hash);
}

void MainWindow::showCommitDetails(const QString &hash)
{
    if (!m_repo.isValid() || hash.isEmpty()) {
        m_detailsPanel->clear();
        return;
    }

    const CommitDetails details = m_git.commitDetails(m_repo.path(), hash);
    if (details.commit.hash.isEmpty() && !m_git.lastError().isEmpty()) {
        setStatusMessage(m_git.lastError());
        return;
    }
    m_detailsPanel->showDetails(details);
}

void MainWindow::onBranchSelected(int row)
{
    if (!m_repo.isValid() || row < 0) {
        m_branchFilter.clear();
        if (m_repo.isValid()) {
            reloadLog();
        }
        return;
    }

    const Branch branch = branchAtRow(row);
    reloadLog(branch.name);
}

Branch MainWindow::branchAtRow(int row) const
{
    if (row >= 0 && row < static_cast<int>(m_branches.size())) {
        return m_branches[static_cast<size_t>(row)];
    }
    return {};
}

void MainWindow::mergeSelectedBranch()
{
    if (!m_repo.isValid()) {
        return;
    }

    const int row = m_branchList->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Merge"), tr("Select a branch first."));
        return;
    }

    const Branch target = branchAtRow(row);
    if (target.name.isEmpty()) {
        return;
    }

    if (target.isCurrent) {
        QMessageBox::information(this, tr("Merge"), tr("Cannot merge a branch into itself."));
        return;
    }

    if (m_git.hasUncommittedChanges(m_repo.path())) {
        const auto answer = QMessageBox::warning(
            this, tr("Merge"),
            tr("The working tree has uncommitted changes. Continue anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Merge"));
    auto *layout = new QVBoxLayout(&dialog);
    const QString current = m_git.currentBranch(m_repo.path());
    layout->addWidget(new QLabel(tr("Merge \"%1\" into \"%2\"?").arg(target.name, current),
                                 &dialog));

    auto *noFfCheck = new QCheckBox(tr("Create a merge commit even when fast-forward is possible"),
                                    &dialog);
    layout->addWidget(noFfCheck);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const GitProcessResult result =
        m_git.merge(m_repo.path(), target.name, noFfCheck->isChecked());
    if (result.success()) {
        QMessageBox::information(this, tr("Merge"), tr("Merge completed successfully."));
        refreshRepository();
        return;
    }

    const QStringList conflictFiles = m_git.unmergedFiles(m_repo.path());
    QString message = m_git.lastError();
    if (!conflictFiles.isEmpty()) {
        message += QStringLiteral("\n\n") + tr("Conflicting files:") + QStringLiteral("\n")
                   + conflictFiles.join(QStringLiteral("\n"));
    }

    QMessageBox::critical(this, tr("Merge failed"), message);

    const auto abortAnswer = QMessageBox::question(
        this, tr("Merge failed"), tr("Abort the merge?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (abortAnswer == QMessageBox::Yes) {
        const GitProcessResult abortResult = m_git.mergeAbort(m_repo.path());
        if (abortResult.success()) {
            QMessageBox::information(this, tr("Merge"), tr("Merge aborted."));
            refreshRepository();
        } else {
            QMessageBox::warning(this, tr("Merge"),
                                 tr("Could not abort merge:\n%1").arg(m_git.lastError()));
        }
    }
}

void MainWindow::loadRecentRepos()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("recent"));
    const QStringList recent = settings.value(QStringLiteral("repos")).toStringList();
    settings.endGroup();
    if (!recent.isEmpty() && !m_repo.isValid()) {
        setStatusMessage(tr("Recent: %1").arg(recent.first()));
    }
}

void MainWindow::saveRecentRepo(const QString &path)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("recent"));
    QStringList recent = settings.value(QStringLiteral("repos")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10) {
        recent.removeLast();
    }
    settings.setValue(QStringLiteral("repos"), recent);
    settings.endGroup();
}

void MainWindow::setStatusMessage(const QString &message)
{
    m_statusLabel->setText(message);
}
