#pragma once

#include "git/GitService.h"

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QListWidget;

class RemotesDialog : public QDialog {
    Q_OBJECT

public:
    explicit RemotesDialog(const QString &repoPath, GitService *git, QWidget *parent = nullptr);

    bool wasModified() const { return m_modified; }

private slots:
    void onRemoteSelectionChanged();
    void onSeparatePushToggled(bool checked);
    void onAddRemote();
    void onSaveRemote();
    void onRemoveRemote();

private:
    void reloadRemotes();
    void clearForm();
    void fillForm(const GitRemote &remote);
    GitRemote remoteFromForm() const;
    bool validateForm(QString *errorOut) const;

    QString m_repoPath;
    GitService *m_git = nullptr;
    bool m_modified = false;
    bool m_addMode = false;

    QListWidget *m_remoteList = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_fetchUrlEdit = nullptr;
    QCheckBox *m_separatePushCheck = nullptr;
    QLineEdit *m_pushUrlEdit = nullptr;
};
