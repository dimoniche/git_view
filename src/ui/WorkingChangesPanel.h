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

signals:
    void commitRequested();

private slots:
    void onFileSelectionChanged();
    void onScopeChanged();

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
    QComboBox *m_scopeCombo = nullptr;
    QListWidget *m_filesList = nullptr;
    QLabel *m_diffTitle = nullptr;
    QPlainTextEdit *m_diffView = nullptr;
};
