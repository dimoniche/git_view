#pragma once

#include "core/Branch.h"
#include "core/Repo.h"

#include <QPoint>
#include "git/GitService.h"

#include <QMainWindow>
#include <vector>

class QListWidget;
class QLabel;
class QPushButton;
class CommitHistoryView;
class CommitDetailsPanel;
class WorkingChangesPanel;
class RepoTerminalPanel;
class QScrollArea;
class QTabWidget;
class QSplitter;
class QAction;
class QMenu;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openRepository();
    void createRepository();
    void refreshRepository();
    void onBranchSelected(int row);
    void mergeSelectedBranch();
    void createBranch(const QString &startPointHint = {});
    void focusCommitDetails(const QString &hash);
    void loadMoreCommits();
    void loadAllCommits();
    void filterLogToBranch(int row);
    void showAllCommits();
    void onCommitSelected(const QString &hash);
    void toggleBranchesPanel(bool visible);
    void toggleDetailsPanel(bool visible);
    void focusHistoryPanel();
    void commitChanges();
    void discardAllChanges();
    void discardFileChanges(const QString &path);
    void addPathToGitignore(const QString &path);
    void checkoutBranch(const Branch &branch);
    void checkoutSelectedBranch();
    void deleteBranch(const Branch &branch);
    void publishBranch(const Branch &branch);
    void pushBranch(const Branch &branch);
    bool pushBranchNoPrompt(const Branch &branch);
    Branch currentLocalBranch() const;
    void pullBranch(const Branch &branch);
    void pullCurrentBranch();
    void fetchRemotes();
    void publishOrPushSelectedBranch();
    void configureRemotes();
    void configureGitIdentity();
    void showBranchContextMenu(const QPoint &pos);

private:
    void setupUi();
    void loadRecentRepos();
    void restoreLastRepository();
    void updateRecentReposMenu();
    void clearRecentRepos();
    void removeInvalidRecentRepo(const QString &path);
    QStringList recentRepos() const;
    void saveRecentRepo(const QString &path);
    void restoreWindowLayout();
    void saveWindowLayout();
    void resetLayout();
    void applySplitterStyle(QSplitter *splitter);
    void showWorkingTreeTab();
    void showTerminalPanel();
    void toggleTerminalPanel(bool visible);
    void setRepository(const QString &path);
    void reloadBranches();
    void reloadLog(const QString &branchFilter = {});
    void showCommitDetails(const QString &hash);
    void refreshWorkingTree();
    void updateWorkingTreeActions();
    void updateBranchActions();
    void updateRepoLabel();
    void setStatusMessage(const QString &message);
    Branch branchAtRow(int row) const;
    Branch branchForActions() const;
    int rowForBranch(const QString &name) const;
    int currentBranchRow() const;
    void selectBranchRow(int row, bool updateLog);
    void selectCurrentBranch();
    void restoreBranchListSelection();
    QString pickRemoteForBranch(const Branch &branch, const QString &title);
    QString pickRemoteForFetch(const QString &title);
    QString promptAddRemote(const QString &title);
    bool promptGitIdentity(const QString &reason = {});
    bool ensureRemoteAuthentication(const QString &remote, const QString &reason = {});
    bool promptRemoteCredentials(const QString &remote, const QString &reason = {});
    bool executePush(const Branch &branch, const QString &remote, bool setUpstream,
                     const QString &failureTitle);

    GitService m_git;
    Repo m_repo;
    std::vector<Branch> m_branches;
    QString m_branchFilter;
    int m_logLimit = 500;

    QLabel *m_repoLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QListWidget *m_branchList = nullptr;
    CommitHistoryView *m_historyView = nullptr;
    CommitDetailsPanel *m_detailsPanel = nullptr;
    WorkingChangesPanel *m_workingPanel = nullptr;
    QTabWidget *m_detailsTabs = nullptr;
    QPushButton *m_checkoutButton = nullptr;
    QPushButton *m_createBranchButton = nullptr;
    QPushButton *m_publishBranchButton = nullptr;
    QPushButton *m_fetchButton = nullptr;
    QPushButton *m_pullButton = nullptr;
    QPushButton *m_mergeButton = nullptr;
    QPushButton *m_remotesButton = nullptr;
    QPushButton *m_showAllCommitsButton = nullptr;
    QPushButton *m_loadMoreButton = nullptr;
    QPushButton *m_loadAllButton = nullptr;

    QSplitter *m_rootSplitter = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    RepoTerminalPanel *m_terminalPanel = nullptr;
    QWidget *m_branchPanel = nullptr;
    QWidget *m_detailsPanelContainer = nullptr;

    QAction *m_toggleBranchesAction = nullptr;
    QAction *m_toggleDetailsAction = nullptr;
    QAction *m_commitAction = nullptr;
    QAction *m_discardAllAction = nullptr;
    QAction *m_discardFileAction = nullptr;
    QAction *m_checkoutAction = nullptr;
    QAction *m_publishBranchAction = nullptr;
    QAction *m_fetchAction = nullptr;
    QAction *m_pullAction = nullptr;
    QAction *m_toggleTerminalAction = nullptr;
    QMenu *m_recentReposMenu = nullptr;
    bool m_syncingBranchList = false;
};
