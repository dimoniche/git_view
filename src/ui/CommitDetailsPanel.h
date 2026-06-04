#pragma once

#include "core/CommitDetails.h"

#include <QString>

#include <QWidget>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class GitService;

class CommitDetailsPanel : public QWidget {
    Q_OBJECT

public:
    explicit CommitDetailsPanel(QWidget *parent = nullptr);

    void setRepoContext(const QString &repoPath, GitService *git);
    void showDetails(const CommitDetails &details);
    void clear();

private slots:
    void onFileSelectionChanged();
    void showFilesContextMenu(const QPoint &pos);

private:
    void loadDiffForCurrentFile();
    void showDiffText(const QString &text, const QString &title);

    QString m_repoPath;
    GitService *m_git = nullptr;
    QString m_commitHash;
    CommitDetails m_details;

    QLabel *m_summaryLabel = nullptr;
    QListWidget *m_filesList = nullptr;
    QLabel *m_diffTitle = nullptr;
    QPlainTextEdit *m_diffView = nullptr;
};
