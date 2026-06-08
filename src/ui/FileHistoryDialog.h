#pragma once

#include "core/Commit.h"
#include "git/GitService.h"

#include <QString>
#include <QWidget>
#include <vector>

class QListWidget;
class DiffViewerWidget;

class FileHistoryDialog : public QWidget {
    Q_OBJECT

public:
    static void open(QWidget *parent, GitService *git, const QString &repoPath,
                     const QString &filePath);

private:
    explicit FileHistoryDialog(GitService *git, const QString &repoPath,
                               const QString &filePath, QWidget *parent = nullptr);

    void populateCommitList(const std::vector<Commit> &commits);
    void loadDiffForCommit(const QString &hash);

    GitService *m_git = nullptr;
    QString m_repoPath;
    QString m_filePath;

    QListWidget *m_commitList = nullptr;
    DiffViewerWidget *m_diffViewer = nullptr;
};
