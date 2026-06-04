#pragma once

#include "core/Branch.h"
#include "core/Repo.h"
#include "git/GitService.h"

#include <QMainWindow>
#include <vector>

class QListWidget;
class QLabel;
class QPushButton;
class CommitHistoryView;
class CommitDetailsPanel;
class QScrollArea;
class QDockWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openRepository();
    void refreshRepository();
    void onBranchSelected(int row);
    void mergeSelectedBranch();
    void loadMoreCommits();
    void onCommitSelected(const QString &hash);

private:
    void setupUi();
    void loadRecentRepos();
    void saveRecentRepo(const QString &path);
    void restoreWindowLayout();
    void saveWindowLayout();
    void setRepository(const QString &path);
    void reloadBranches();
    void reloadLog(const QString &branchFilter = {});
    void showCommitDetails(const QString &hash);
    void setStatusMessage(const QString &message);
    Branch branchAtRow(int row) const;

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
    QScrollArea *m_historyScroll = nullptr;
    QPushButton *m_mergeButton = nullptr;
    QPushButton *m_loadMoreButton = nullptr;
    QDockWidget *m_branchesDock = nullptr;
    QDockWidget *m_detailsDock = nullptr;
};
