#pragma once

#include "core/WorkingTreeChange.h"
#include "git/GitService.h"

#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;
class SourceCodeView;

class WorkingFileEditorDialog : public QWidget {
    Q_OBJECT

public:
    using SavedCallback = std::function<void()>;

    struct OpenCheck {
        bool canOpen = false;
        bool binary = false;
        QString error;
    };

    static OpenCheck checkEditable(GitService *git, const QString &repoPath,
                                   const QString &relativePath, const WorkingTreeChange &change,
                                   WorkingDiffScope scope);

    static void open(QWidget *parent, GitService *git, const QString &repoPath,
                     const QString &relativePath, const WorkingTreeChange &change,
                     WorkingDiffScope scope, SavedCallback onSaved = {});

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    WorkingFileEditorDialog(GitService *git, const QString &repoPath, const QString &relativePath,
                            const WorkingTreeChange &change, WorkingDiffScope scope,
                            SavedCallback onSaved, QWidget *parent = nullptr);

    bool loadContent();
    bool saveToDisk();
    bool confirmDiscard();
    void updateSaveState();
    QString absolutePath() const;
    OpenCheck inspectContent() const;

    GitService *m_git = nullptr;
    QString m_repoPath;
    QString m_relativePath;
    WorkingTreeChange m_change;
    WorkingDiffScope m_scope = WorkingDiffScope::Unstaged;
    SavedCallback m_onSaved;
    QString m_loadedContent;
    bool m_dirty = false;

    class SourceCodeView *m_editor = nullptr;
    class QLabel *m_statusLabel = nullptr;
    class QPushButton *m_saveButton = nullptr;
};
