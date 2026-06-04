#pragma once

#include "core/WorkingTreeChange.h"
#include "git/GitService.h"

#include <QWidget>
#include <vector>

class QLabel;
class QPushButton;
class QListWidget;
class QPlainTextEdit;
class QComboBox;

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
    void fileSelectionChanged();

private slots:
    void onFileSelectionChanged();
    void onScopeChanged();
    void onDiscardFileClicked();
    void onDiscardAllClicked();
    void updateDiscardFileButton();
    void showFilesContextMenu(const QPoint &pos);

private:
    void loadDiffForCurrentFile();
    void showDiffText(const QString &text, const QString &title);
    void applyBestScopeForChange(const WorkingTreeChange &change);
    WorkingDiffScope currentScope() const;

    QString m_repoPath;
    GitService *m_git = nullptr;
    std::vector<WorkingTreeChange> m_changes;

    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_commitButton = nullptr;
    QPushButton *m_discardFileButton = nullptr;
    QPushButton *m_discardAllButton = nullptr;
    QComboBox *m_scopeCombo = nullptr;
    QListWidget *m_filesList = nullptr;
    QLabel *m_diffTitle = nullptr;
    QPlainTextEdit *m_diffView = nullptr;
};
