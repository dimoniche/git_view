#include "ui/MainWindow.h"

#include "core/WorkingTreeChange.h"
#include "ui/CommitDetailsPanel.h"
#include "ui/CommitHistoryView.h"
#include "ui/WorkingChangesPanel.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr int kInitialLogLimit = 500;
constexpr int kLogPageSize = 500;
constexpr int kSplitterHandleWidth = 14;
constexpr int kDefaultBranchWidth = 260;
constexpr int kDefaultDetailsWidth = 420;

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

    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->addAction(openAction);
    toolbar->addAction(refreshAction);
    toolbar->addSeparator();

    m_toggleBranchesAction = toolbar->addAction(tr("Branches"));
    m_toggleBranchesAction->setCheckable(true);
    m_toggleBranchesAction->setChecked(true);
    m_toggleBranchesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(m_toggleBranchesAction, &QAction::toggled, this, &MainWindow::toggleBranchesPanel);

    m_toggleDetailsAction = toolbar->addAction(tr("Details"));
    m_toggleDetailsAction->setCheckable(true);
    m_toggleDetailsAction->setChecked(true);
    m_toggleDetailsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(m_toggleDetailsAction, &QAction::toggled, this, &MainWindow::toggleDetailsPanel);

    m_commitAction = toolbar->addAction(tr("Commit…"));
    m_commitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    m_commitAction->setEnabled(false);
    connect(m_commitAction, &QAction::triggered, this, &MainWindow::commitChanges);

    auto *newBranchAction = new QAction(tr("New branch…"), this);
    newBranchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    connect(newBranchAction, &QAction::triggered, this, [this]() { createBranch(); });

    m_discardAllAction = new QAction(tr("Discard all changes…"), this);
    m_discardAllAction->setEnabled(false);
    connect(m_discardAllAction, &QAction::triggered, this, &MainWindow::discardAllChanges);

    m_discardFileAction = new QAction(tr("Discard file changes…"), this);
    m_discardFileAction->setEnabled(false);
    connect(m_discardFileAction, &QAction::triggered, this, [this]() {
        showWorkingTreeTab();
        if (m_workingPanel && m_workingPanel->hasSelectedChange()) {
            discardFileChanges(m_workingPanel->selectedFilePath());
        }
    });

    auto *repoMenu = menuBar()->addMenu(tr("&Repository"));
    repoMenu->addAction(newBranchAction);
    repoMenu->addAction(m_commitAction);
    repoMenu->addSeparator();
    repoMenu->addAction(m_discardFileAction);
    repoMenu->addAction(m_discardAllAction);

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(6);

    m_repoLabel = new QLabel(tr("No repository"), central);
    m_repoLabel->setTextFormat(Qt::RichText);
    m_repoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_repoLabel->setOpenExternalLinks(false);
    connect(m_repoLabel, &QLabel::linkActivated, this, [this](const QString &link) {
        if (link == QStringLiteral("working")) {
            showWorkingTreeTab();
        }
    });
    rootLayout->addWidget(m_repoLabel);

    m_mainSplitter = new QSplitter(Qt::Horizontal, central);
    applySplitterStyle(m_mainSplitter);

    m_branchPanel = new QWidget(m_mainSplitter);
    m_branchPanel->setMinimumWidth(180);
    auto *branchLayout = new QVBoxLayout(m_branchPanel);
    branchLayout->setContentsMargins(4, 4, 4, 4);
    branchLayout->addWidget(new QLabel(tr("Branches"), m_branchPanel));
    m_branchList = new QListWidget(m_branchPanel);
    m_branchList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_branchList, &QListWidget::currentRowChanged, this, &MainWindow::onBranchSelected);
    connect(m_branchList, &QWidget::customContextMenuRequested, this,
            &MainWindow::showBranchContextMenu);
    branchLayout->addWidget(m_branchList, 1);
    m_createBranchButton = new QPushButton(tr("New branch…"), m_branchPanel);
    m_createBranchButton->setEnabled(false);
    connect(m_createBranchButton, &QPushButton::clicked, this, [this]() { createBranch(); });
    branchLayout->addWidget(m_createBranchButton);
    m_mergeButton = new QPushButton(tr("Merge into current…"), m_branchPanel);
    m_mergeButton->setEnabled(false);
    connect(m_mergeButton, &QPushButton::clicked, this, &MainWindow::mergeSelectedBranch);
    branchLayout->addWidget(m_mergeButton);
    m_mainSplitter->addWidget(m_branchPanel);

    auto *historyColumn = new QWidget(m_mainSplitter);
    auto *historyLayout = new QVBoxLayout(historyColumn);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    m_historyScroll = new QScrollArea(historyColumn);
    m_historyScroll->setWidgetResizable(true);
    m_historyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyScroll->setFrameShape(QFrame::StyledPanel);
    m_historyView = new CommitHistoryView;
    connect(m_historyView, &CommitHistoryView::commitSelected, this,
            &MainWindow::onCommitSelected);
    connect(m_historyView, &CommitHistoryView::viewCommitDetailsRequested, this,
            &MainWindow::focusCommitDetails);
    connect(m_historyView, &CommitHistoryView::createBranchFromCommitRequested, this,
            [this](const QString &hash) { createBranch(hash); });
    m_historyScroll->setWidget(m_historyView);
    historyLayout->addWidget(m_historyScroll, 1);
    m_loadMoreButton = new QPushButton(tr("Load more commits…"), historyColumn);
    m_loadMoreButton->setEnabled(false);
    connect(m_loadMoreButton, &QPushButton::clicked, this, &MainWindow::loadMoreCommits);
    historyLayout->addWidget(m_loadMoreButton);
    m_mainSplitter->addWidget(historyColumn);

    m_detailsPanelContainer = new QWidget(m_mainSplitter);
    m_detailsPanelContainer->setMinimumWidth(300);
    auto *detailsLayout = new QVBoxLayout(m_detailsPanelContainer);
    detailsLayout->setContentsMargins(4, 4, 4, 4);
    m_detailsPanel = new CommitDetailsPanel(m_detailsPanelContainer);
    m_workingPanel = new WorkingChangesPanel(m_detailsPanelContainer);
    connect(m_workingPanel, &WorkingChangesPanel::commitRequested, this, &MainWindow::commitChanges);
    connect(m_workingPanel, &WorkingChangesPanel::discardAllRequested, this, &MainWindow::discardAllChanges);
    connect(m_workingPanel, &WorkingChangesPanel::discardFileRequested, this,
            &MainWindow::discardFileChanges);
    connect(m_workingPanel, &WorkingChangesPanel::fileSelectionChanged, this,
            &MainWindow::updateWorkingTreeActions);
    m_detailsTabs = new QTabWidget(m_detailsPanelContainer);
    m_detailsTabs->addTab(m_detailsPanel, tr("Commit"));
    m_detailsTabs->addTab(m_workingPanel, tr("Working tree"));
    connect(m_detailsTabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_detailsTabs->widget(index) == m_workingPanel) {
            refreshWorkingTree();
        }
    });
    detailsLayout->addWidget(m_detailsTabs);
    m_mainSplitter->addWidget(m_detailsPanelContainer);

    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    m_mainSplitter->setSizes({kDefaultBranchWidth, 700, kDefaultDetailsWidth});

    rootLayout->addWidget(m_mainSplitter, 1);

    m_statusLabel = new QLabel(central);
    rootLayout->addWidget(m_statusLabel);

    setCentralWidget(central);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_toggleBranchesAction);
    viewMenu->addAction(m_toggleDetailsAction);

    auto *workingTabAction = new QAction(tr("Working tree changes"), this);
    workingTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(workingTabAction, &QAction::triggered, this, &MainWindow::showWorkingTreeTab);
    viewMenu->addAction(workingTabAction);

    auto *focusHistoryAction = new QAction(tr("Focus commit history"), this);
    focusHistoryAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(focusHistoryAction, &QAction::triggered, this, &MainWindow::focusHistoryPanel);
    viewMenu->addAction(focusHistoryAction);

    viewMenu->addSeparator();
    viewMenu->addAction(tr("Reset layout"), this, &MainWindow::resetLayout);

    restoreWindowLayout();
}

void MainWindow::applySplitterStyle(QSplitter *splitter)
{
    if (!splitter) {
        return;
    }
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(kSplitterHandleWidth);
    splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle:horizontal {"
        "  background-color: #aeb6c2;"
        "  border-left: 1px solid #8a939f;"
        "  border-right: 1px solid #8a939f;"
        "}"
        "QSplitter::handle:horizontal:hover {"
        "  background-color: #3d7ab8;"
        "}"));
}

void MainWindow::toggleBranchesPanel(bool visible)
{
    if (!m_branchPanel) {
        return;
    }
    m_branchPanel->setVisible(visible);
    if (visible && m_mainSplitter) {
        QList<int> sizes = m_mainSplitter->sizes();
        if (sizes.size() >= 3 && sizes[0] < 80) {
            sizes[0] = kDefaultBranchWidth;
            m_mainSplitter->setSizes(sizes);
        }
    }
}

void MainWindow::toggleDetailsPanel(bool visible)
{
    if (!m_detailsPanelContainer) {
        return;
    }
    m_detailsPanelContainer->setVisible(visible);
    if (visible && m_mainSplitter) {
        QList<int> sizes = m_mainSplitter->sizes();
        if (sizes.size() >= 3 && sizes[2] < 80) {
            sizes[2] = kDefaultDetailsWidth;
            m_mainSplitter->setSizes(sizes);
        }
    }
}

void MainWindow::focusHistoryPanel()
{
    if (m_toggleBranchesAction) {
        m_toggleBranchesAction->setChecked(false);
    }
    if (m_toggleDetailsAction) {
        m_toggleDetailsAction->setChecked(false);
    }
}

void MainWindow::showWorkingTreeTab()
{
    if (m_toggleDetailsAction) {
        m_toggleDetailsAction->setChecked(true);
    }
    if (m_detailsTabs && m_workingPanel) {
        m_detailsTabs->setCurrentWidget(m_workingPanel);
        refreshWorkingTree();
    }
}

void MainWindow::resetLayout()
{
    QSettings settings;
    settings.remove(QStringLiteral("windowGeometry"));
    settings.remove(QStringLiteral("splitterState"));
    settings.remove(QStringLiteral("windowState"));
    resize(1200, 800);
    if (m_toggleBranchesAction) {
        m_toggleBranchesAction->setChecked(true);
    }
    if (m_toggleDetailsAction) {
        m_toggleDetailsAction->setChecked(true);
    }
    if (m_mainSplitter) {
        m_mainSplitter->setSizes({kDefaultBranchWidth, 700, kDefaultDetailsWidth});
    }
}

void MainWindow::restoreWindowLayout()
{
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("windowGeometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    const QByteArray splitterState = settings.value(QStringLiteral("splitterState")).toByteArray();
    if (!splitterState.isEmpty() && m_mainSplitter) {
        m_mainSplitter->restoreState(splitterState);
    }

    const bool branchesVisible = settings.value(QStringLiteral("branchesVisible"), true).toBool();
    const bool detailsVisible = settings.value(QStringLiteral("detailsVisible"), true).toBool();
    if (m_toggleBranchesAction) {
        m_toggleBranchesAction->setChecked(branchesVisible);
    }
    if (m_toggleDetailsAction) {
        m_toggleDetailsAction->setChecked(detailsVisible);
    }
}

void MainWindow::saveWindowLayout()
{
    QSettings settings;
    settings.setValue(QStringLiteral("windowGeometry"), saveGeometry());
    if (m_mainSplitter) {
        settings.setValue(QStringLiteral("splitterState"), m_mainSplitter->saveState());
    }
    if (m_toggleBranchesAction) {
        settings.setValue(QStringLiteral("branchesVisible"), m_toggleBranchesAction->isChecked());
    }
    if (m_toggleDetailsAction) {
        settings.setValue(QStringLiteral("detailsVisible"), m_toggleDetailsAction->isChecked());
    }
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
    refreshWorkingTree();
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
    m_detailsPanel->setRepoContext(topLevel, &m_git);
    m_workingPanel->setRepoContext(topLevel, &m_git);
    m_logLimit = kInitialLogLimit;
    m_branchFilter.clear();
    saveRecentRepo(topLevel);

    m_branchList->clearSelection();
    reloadBranches();
    reloadLog();
    refreshWorkingTree();

    if (m_git.hasUncommittedChanges(topLevel)) {
        m_detailsTabs->setCurrentWidget(m_workingPanel);
    }

    m_createBranchButton->setEnabled(true);
    m_mergeButton->setEnabled(true);
    m_loadMoreButton->setEnabled(true);
    updateWorkingTreeActions();
    setStatusMessage(tr("Loaded %1").arg(topLevel));
}

void MainWindow::commitChanges()
{
    if (!m_repo.isValid()) {
        QMessageBox::information(this, tr("Commit"), tr("Open a repository first."));
        return;
    }

    const QStringList conflicts = m_git.unmergedFiles(m_repo.path());
    if (!conflicts.isEmpty()) {
        QMessageBox::critical(
            this, tr("Commit"),
            tr("Cannot commit: resolve merge conflicts first.\n\n%1").arg(conflicts.join(QLatin1Char('\n'))));
        return;
    }

    if (!m_git.hasUncommittedChanges(m_repo.path())) {
        QMessageBox::information(this, tr("Commit"), tr("There are no changes to commit."));
        return;
    }

    showWorkingTreeTab();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Commit changes"));
    dialog.resize(520, 360);

    auto *layout = new QVBoxLayout(&dialog);

    auto *stageAllRadio =
        new QRadioButton(tr("Stage all changes and commit (git add -A)"), &dialog);
    auto *stagedOnlyRadio = new QRadioButton(tr("Commit staged changes only"), &dialog);
    stageAllRadio->setChecked(true);
    if (!m_git.hasStagedChanges(m_repo.path())) {
        stagedOnlyRadio->setEnabled(false);
    }
    layout->addWidget(stageAllRadio);
    layout->addWidget(stagedOnlyRadio);

    layout->addWidget(new QLabel(tr("Commit message:"), &dialog));
    auto *messageEdit = new QPlainTextEdit(&dialog);
    messageEdit->setPlaceholderText(tr("Describe your changes"));
    messageEdit->setMinimumHeight(140);
    layout->addWidget(messageEdit, 1);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString message = messageEdit->toPlainText().trimmed();
    if (message.isEmpty()) {
        QMessageBox::warning(this, tr("Commit"), tr("Commit message cannot be empty."));
        return;
    }

    if (stageAllRadio->isChecked()) {
        const GitProcessResult stageResult = m_git.stageAll(m_repo.path());
        if (!stageResult.success()) {
            QMessageBox::critical(this, tr("Commit"), m_git.lastError());
            return;
        }
    } else if (!m_git.hasStagedChanges(m_repo.path())) {
        QMessageBox::warning(this, tr("Commit"), tr("Nothing is staged. Stage changes first or use \"Stage all\"."));
        return;
    }

    const GitProcessResult commitResult = m_git.commit(m_repo.path(), message);
    if (!commitResult.success()) {
        QMessageBox::critical(
            this, tr("Commit failed"),
            tr("%1\n\n%2").arg(m_git.lastError(), commitResult.stdoutText.trimmed()));
        return;
    }

    const QString output = commitResult.stdoutText.trimmed();
    QMessageBox::information(
        this, tr("Commit"),
        output.isEmpty() ? tr("Commit created successfully.") : output);
    refreshRepository();
}

void MainWindow::discardAllChanges()
{
    if (!m_repo.isValid()) {
        QMessageBox::information(this, tr("Discard changes"), tr("Open a repository first."));
        return;
    }

    const QStringList conflicts = m_git.unmergedFiles(m_repo.path());
    if (!conflicts.isEmpty()) {
        QMessageBox::critical(
            this, tr("Discard changes"),
            tr("Cannot discard while merge conflicts are unresolved.\nUse \"Merge abort\" or resolve conflicts first.\n\n%1")
                .arg(conflicts.join(QLatin1Char('\n'))));
        return;
    }

    if (!m_git.hasUncommittedChanges(m_repo.path())) {
        QMessageBox::information(this, tr("Discard changes"), tr("There are no changes to discard."));
        return;
    }

    showWorkingTreeTab();

    const auto answer = QMessageBox::warning(
        this, tr("Discard all changes"),
        tr("Permanently discard ALL uncommitted changes?\n\n"
           "• Tracked files will be reset to the last commit (git reset --hard HEAD)\n"
           "• Untracked files and folders will be deleted (git clean -fd)\n\n"
           "This cannot be undone."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const GitProcessResult result = m_git.discardAllChanges(m_repo.path());
    if (!result.success()) {
        QMessageBox::critical(
            this, tr("Discard failed"),
            tr("%1\n\n%2").arg(m_git.lastError(), result.stderrText.trimmed()));
        return;
    }

    QMessageBox::information(this, tr("Discard changes"), tr("All uncommitted changes were discarded."));
    QTimer::singleShot(0, this, &MainWindow::refreshRepository);
}

void MainWindow::discardFileChanges(const QString &path)
{
    if (!m_repo.isValid()) {
        QMessageBox::information(this, tr("Discard changes"), tr("Open a repository first."));
        return;
    }

    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Discard changes"), tr("Select a file in the list first."));
        return;
    }

    const QStringList conflicts = m_git.unmergedFiles(m_repo.path());
    if (conflicts.contains(path)) {
        QMessageBox::critical(
            this, tr("Discard changes"),
            tr("Cannot discard \"%1\" while it has merge conflicts.").arg(path));
        return;
    }

    WorkingTreeChange change;
    bool found = false;
    for (const WorkingTreeChange &candidate : m_git.workingTreeChanges(m_repo.path())) {
        if (candidate.path == path) {
            change = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        QMessageBox::information(this, tr("Discard changes"),
                                 tr("File \"%1\" has no pending changes.").arg(path));
        QTimer::singleShot(0, this, &MainWindow::refreshRepository);
        return;
    }

    showWorkingTreeTab();

    QString actionText;
    if (change.isUntracked()) {
        actionText = tr("Delete untracked path \"%1\"?").arg(path);
    } else {
        actionText =
            tr("Discard all changes to \"%1\" (staged and unstaged) and restore the last committed version?")
                .arg(path);
    }

    const auto answer =
        QMessageBox::warning(this, tr("Discard file changes"), actionText,
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const GitProcessResult result = m_git.discardFileChanges(m_repo.path(), change);
    if (!result.success()) {
        QMessageBox::critical(
            this, tr("Discard failed"),
            tr("%1\n\n%2").arg(m_git.lastError(), result.stderrText.trimmed()));
        return;
    }

    QMessageBox::information(this, tr("Discard changes"),
                             tr("Changes to \"%1\" were discarded.").arg(path));
    QTimer::singleShot(0, this, &MainWindow::refreshRepository);
}

void MainWindow::updateWorkingTreeActions()
{
    if (!m_workingPanel) {
        return;
    }
    const bool repoOpen = m_repo.isValid();
    const bool dirty = repoOpen && m_git.hasUncommittedChanges(m_repo.path());
    if (m_discardAllAction) {
        m_discardAllAction->setEnabled(repoOpen && dirty);
    }
    if (m_discardFileAction) {
        m_discardFileAction->setEnabled(repoOpen && dirty && m_workingPanel->hasSelectedChange());
    }
    m_workingPanel->setCommitEnabled(repoOpen);
}

void MainWindow::updateRepoLabel()
{
    if (!m_repo.isValid()) {
        m_repoLabel->setText(tr("No repository"));
        return;
    }

    const QString branch = m_git.currentBranch(m_repo.path());
    const bool dirty = m_git.hasUncommittedChanges(m_repo.path());
    QString html = tr("Repository: %1 — branch: %2")
                       .arg(m_repo.path().toHtmlEscaped(), branch.toHtmlEscaped());
    if (dirty) {
        html += QStringLiteral(" — <a href=\"working\">") + tr("uncommitted changes")
                + QStringLiteral("</a>");
    }
    m_repoLabel->setText(html);
}

void MainWindow::refreshWorkingTree()
{
    if (!m_repo.isValid()) {
        return;
    }

    m_workingPanel->refresh();
    const bool dirty = m_git.hasUncommittedChanges(m_repo.path());
    const int workingTabIndex = m_detailsTabs->indexOf(m_workingPanel);
    if (workingTabIndex >= 0) {
        m_detailsTabs->setTabText(workingTabIndex,
                                  dirty ? tr("Working tree *") : tr("Working tree"));
    }
    updateWorkingTreeActions();
    updateRepoLabel();
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

void MainWindow::focusCommitDetails(const QString &hash)
{
    showCommitDetails(hash);
    if (m_detailsTabs && m_detailsPanel) {
        m_detailsTabs->setCurrentWidget(m_detailsPanel);
    }
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

void MainWindow::checkoutBranch(const Branch &branch)
{
    if (!m_repo.isValid() || branch.name.isEmpty()) {
        return;
    }

    if (branch.isCurrent) {
        return;
    }

    if (m_git.hasUncommittedChanges(m_repo.path())) {
        const auto answer = QMessageBox::warning(
            this, tr("Checkout branch"),
            tr("The working tree has uncommitted changes. Switch to \"%1\" anyway?").arg(branch.name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    const GitProcessResult result = m_git.checkoutBranch(m_repo.path(), branch.name);
    if (!result.success()) {
        QMessageBox::critical(
            this, tr("Checkout failed"),
            tr("%1\n\n%2").arg(m_git.lastError(), result.stderrText.trimmed()));
        return;
    }

    refreshRepository();
}

void MainWindow::showBranchContextMenu(const QPoint &pos)
{
    if (!m_repo.isValid()) {
        return;
    }

    QListWidgetItem *item = m_branchList->itemAt(pos);
    if (!item) {
        return;
    }

    const int row = m_branchList->row(item);
    if (row < 0) {
        return;
    }

    m_branchList->setCurrentRow(row);
    const Branch branch = branchAtRow(row);
    if (branch.name.isEmpty()) {
        return;
    }

    QMenu menu(this);

    if (!branch.isCurrent) {
        menu.addAction(tr("Checkout \"%1\"…").arg(branch.name), this,
                       [this, branch]() { checkoutBranch(branch); });
    }

    const QString current = m_git.currentBranch(m_repo.path());
    if (!branch.isCurrent && branch.name != current) {
        menu.addAction(tr("Merge into current (%1)…").arg(current), this,
                       &MainWindow::mergeSelectedBranch);
    }

    menu.addAction(tr("Show commits"), this, [this, row]() { onBranchSelected(row); });

    menu.addSeparator();
    menu.addAction(tr("New branch…"), this, [this]() { createBranch(); });

    menu.exec(m_branchList->mapToGlobal(pos));
}

void MainWindow::createBranch(const QString &startPointHint)
{
    if (!m_repo.isValid()) {
        QMessageBox::information(this, tr("New branch"), tr("Open a repository first."));
        return;
    }

    const int row = m_branchList->currentRow();
    const Branch selected = branchAtRow(row);
    const QString current = m_git.currentBranch(m_repo.path());

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Create branch"));
    dialog.resize(440, 220);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Branch name:"), &dialog));

    auto *nameEdit = new QLineEdit(&dialog);
    if (!selected.name.isEmpty() && !selected.isRemote) {
        nameEdit->setText(selected.name + QStringLiteral("-copy"));
    } else if (!startPointHint.isEmpty()) {
        nameEdit->setText(QStringLiteral("branch-") + startPointHint.left(8));
    }
    layout->addWidget(nameEdit);

    layout->addWidget(new QLabel(tr("Based on:"), &dialog));
    auto *baseCombo = new QComboBox(&dialog);
    int defaultBaseIndex = 0;
    baseCombo->addItem(tr("Current branch (%1)").arg(current), QString());
    if (!selected.name.isEmpty()) {
        baseCombo->addItem(tr("Selected: %1").arg(selected.name), selected.name);
    }
    if (!startPointHint.isEmpty()) {
        baseCombo->addItem(tr("Commit %1").arg(startPointHint.left(8)), startPointHint);
        defaultBaseIndex = baseCombo->count() - 1;
    }
    baseCombo->setCurrentIndex(defaultBaseIndex);
    layout->addWidget(baseCombo);

    auto *checkoutCheck = new QCheckBox(tr("Switch to new branch after creation"), &dialog);
    checkoutCheck->setChecked(true);
    layout->addWidget(checkoutCheck);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString branchName = nameEdit->text().trimmed();
    const QString validation = m_git.validateBranchName(m_repo.path(), branchName);
    if (!validation.isEmpty()) {
        QMessageBox::warning(this, tr("New branch"), validation);
        return;
    }

    if (m_git.branchExists(m_repo.path(), branchName)) {
        QMessageBox::warning(
            this, tr("New branch"),
            tr("Branch \"%1\" already exists. Choose another name or delete the existing branch.")
                .arg(branchName));
        return;
    }

    const QString startPoint = baseCombo->currentData().toString().trimmed();
    const bool checkout = checkoutCheck->isChecked();

    if (checkout && m_git.hasUncommittedChanges(m_repo.path())) {
        const auto answer = QMessageBox::warning(
            this, tr("New branch"),
            tr("The working tree has uncommitted changes. Switch branch anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    GitProcessResult result;
    if (checkout) {
        result = m_git.createBranchAndCheckout(m_repo.path(), branchName, startPoint);
    } else {
        result = m_git.createBranch(m_repo.path(), branchName, startPoint);
    }

    if (!result.success()) {
        QMessageBox::critical(
            this, tr("New branch failed"),
            tr("%1\n\n%2").arg(m_git.lastError(), result.stderrText.trimmed()));
        return;
    }

    const QString output = result.stdoutText.trimmed();
    QMessageBox::information(
        this, tr("New branch"),
        output.isEmpty() ? tr("Branch \"%1\" created.").arg(branchName) : output);
    refreshRepository();
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
