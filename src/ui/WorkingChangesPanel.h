#pragma once

#include "core/WorkingTreeChange.h"
#include "git/GitService.h"
#include "ui/DiffViewerDialog.h"

#include <QWidget>
#include <vector>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;

class WorkingChangesPanel : public QWidget {
    Q_OBJECT

public:
    explicit WorkingChangesPanel(QWidget *parent = nullptr);

    void setRepoContext(const QString &repoPath, GitService *git);
    void refresh();
    void setCommitEnabled(bool enabled);
    void setAmendEnabled(bool enabled);
    bool selectFilePath(const QString &repoRelativePath);
    void showFileDiffInWindow(const QString &repoRelativePath,
                              WorkingDiffScope scope = WorkingDiffScope::Unstaged);
    QString selectedFilePath() const;
    QStringList selectedFilePaths() const;
    bool hasSelectedChange() const;

signals:
    void commitRequested();
    void amendRequested();
    void discardAllRequested();
    void discardFilesRequested(const QStringList &paths);
    void stageFilesRequested(const QStringList &paths);
    void unstageFilesRequested(const QStringList &paths);
    void stageAllRequested();
    void addToGitignoreRequested(const QString &path);
    void fileSelectionChanged();
    void workingTreeChanged();

private slots:
    void onFileSelectionChanged();
    void onFileDoubleClicked(QTreeWidgetItem *item, int column);
    void onDiscardFileClicked();
    void onDiscardAllClicked();
    void onStageFilesClicked();
    void onUnstageFilesClicked();
    void onStageAllClicked();
    void updateCommitButton();
    void updateAmendButton();
    void updateDiscardAllButton();
    void updateDiscardFileButton();
    void updateStageButtons();
    void showFilesContextMenu(const QPoint &pos);

private:
    void loadDiffForCurrentFile();
    void openDiffInSeparateWindow();
    void openFileEditor();
    void showFileDiffInWindowImpl(const QString &repoRelativePath, WorkingDiffScope scope);
    DiffViewerSources buildSourcesForFile(const QString &path, WorkingDiffScope scope,
                                          const WorkingTreeChange &change) const;
    void showDiffText(const QString &text, const QString &title);
    void rebuildChangeTree();
    void selectTreeItem(const QString &path, WorkingDiffScope scope);
    WorkingTreeChange changeForPath(const QString &path) const;
    bool isStagedEntry(const WorkingTreeChange &change) const;
    WorkingDiffScope selectedItemScope() const;
    const QTreeWidgetItem *selectedFileItem() const;
    QList<const QTreeWidgetItem *> selectedFileItems() const;
    void selectedPathsByScope(QStringList *stagedSectionPaths, QStringList *changesSectionPaths) const;

    QString m_repoPath;
    GitService *m_git = nullptr;
    std::vector<WorkingTreeChange> m_allChanges;

    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_commitButton = nullptr;
    QPushButton *m_amendButton = nullptr;
    QPushButton *m_stageButton = nullptr;
    QPushButton *m_unstageButton = nullptr;
    QPushButton *m_stageAllButton = nullptr;
    QPushButton *m_discardFileButton = nullptr;
    QPushButton *m_discardAllButton = nullptr;
    QTreeWidget *m_filesTree = nullptr;
    QLabel *m_diffTitle = nullptr;
    QPlainTextEdit *m_diffView = nullptr;
    bool m_repoActionsEnabled = false;
    bool m_amendEnabled = false;
};
