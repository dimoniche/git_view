#pragma once

#include "core/CommitDetails.h"
#include "git/GitService.h"
#include "ui/DiffViewerDialog.h"

#include <QString>
#include <QWidget>

class QListWidget;
class QSplitter;
class DiffViewerWidget;

class CommitDetailsDialog : public QWidget {
    Q_OBJECT

public:
    static void open(QWidget *parent, GitService *git, const QString &repoPath,
                     const QString &commitHash);

private:
    explicit CommitDetailsDialog(GitService *git, const QString &repoPath,
                                 const CommitDetails &details, QWidget *parent = nullptr);

    void populateFileList();
    void adjustFileListWidth();
    void loadDiffForFile(const QString &path);
    void openDiffInSeparateWindow(const QString &path);
    void openFileHistoryInSeparateWindow(const QString &path);
    void showFilesContextMenu(const QPoint &pos);
    DiffViewerSources buildSourcesForFile(const QString &path) const;

    GitService *m_git = nullptr;
    QString m_repoPath;
    CommitDetails m_details;

    QListWidget *m_filesList = nullptr;
    DiffViewerWidget *m_diffViewer = nullptr;
    QSplitter *m_splitter = nullptr;
};
