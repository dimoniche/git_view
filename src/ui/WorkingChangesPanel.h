#pragma once

#include "core/WorkingTreeChange.h"
#include "git/GitService.h"

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
    QString selectedFilePath() const;
    bool hasSelectedChange() const;

signals:
    void commitRequested();
    void discardAllRequested();
    void discardFileRequested(const QString &path);
    void addToGitignoreRequested(const QString &path);
    void fileSelectionChanged();

private slots:
    void onFileSelectionChanged();
    void onDiscardFileClicked();
    void onDiscardAllClicked();
    void updateDiscardFileButton();
    void showFilesContextMenu(const QPoint &pos);

private:
    void loadDiffForCurrentFile();
    void showDiffText(const QString &text, const QString &title);
    void rebuildChangeTree();
    void selectTreeItem(const QString &path, WorkingDiffScope scope);
    WorkingTreeChange changeForPath(const QString &path) const;
    bool isStagedEntry(const WorkingTreeChange &change) const;
    WorkingDiffScope selectedItemScope() const;
    const QTreeWidgetItem *selectedFileItem() const;

    QString m_repoPath;
    GitService *m_git = nullptr;
    std::vector<WorkingTreeChange> m_allChanges;

    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_commitButton = nullptr;
    QPushButton *m_discardFileButton = nullptr;
    QPushButton *m_discardAllButton = nullptr;
    QTreeWidget *m_filesTree = nullptr;
    QLabel *m_diffTitle = nullptr;
    QPlainTextEdit *m_diffView = nullptr;
};
