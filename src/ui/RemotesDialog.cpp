#include "ui/RemotesDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

RemotesDialog::RemotesDialog(const QString &repoPath, GitService *git, QWidget *parent)
    : QDialog(parent)
    , m_repoPath(repoPath)
    , m_git(git)
{
    setWindowTitle(tr("Remotes"));
    resize(560, 400);

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Configured remotes:"), this));

    m_remoteList = new QListWidget(this);
    m_remoteList->setMinimumHeight(100);
    connect(m_remoteList, &QListWidget::currentRowChanged, this,
            &RemotesDialog::onRemoteSelectionChanged);
    layout->addWidget(m_remoteList);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    layout->addWidget(new QLabel(tr("Name:"), this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("origin"));
    layout->addWidget(m_nameEdit);

    layout->addWidget(new QLabel(tr("Repository URL:"), this));
    m_fetchUrlEdit = new QLineEdit(this);
    m_fetchUrlEdit->setPlaceholderText(tr("https://github.com/user/repo.git"));
    m_fetchUrlEdit->setClearButtonEnabled(true);
    layout->addWidget(m_fetchUrlEdit);

    m_separatePushCheck = new QCheckBox(tr("Separate push URL"), this);
    connect(m_separatePushCheck, &QCheckBox::toggled, this, &RemotesDialog::onSeparatePushToggled);
    layout->addWidget(m_separatePushCheck);

    layout->addWidget(new QLabel(tr("Push URL:"), this));
    m_pushUrlEdit = new QLineEdit(this);
    m_pushUrlEdit->setEnabled(false);
    m_pushUrlEdit->setClearButtonEnabled(true);
    layout->addWidget(m_pushUrlEdit);

    auto *buttonRow = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("Add remote…"), this);
    connect(addButton, &QPushButton::clicked, this, &RemotesDialog::onAddRemote);
    buttonRow->addWidget(addButton);

    auto *saveButton = new QPushButton(tr("Save"), this);
    connect(saveButton, &QPushButton::clicked, this, &RemotesDialog::onSaveRemote);
    buttonRow->addWidget(saveButton);

    auto *removeButton = new QPushButton(tr("Remove"), this);
    connect(removeButton, &QPushButton::clicked, this, &RemotesDialog::onRemoveRemote);
    buttonRow->addWidget(removeButton);

    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::clicked, this, &QDialog::accept);
    layout->addWidget(buttons);

    reloadRemotes();
    enterAddMode();
    m_nameEdit->setFocus();
}

void RemotesDialog::reloadRemotes()
{
    m_syncingList = true;

    QItemSelectionModel *selection = m_remoteList->selectionModel();
    if (selection) {
        selection->blockSignals(true);
    }
    m_remoteList->blockSignals(true);

    m_remoteList->clear();
    if (m_git) {
        for (const GitRemote &remote : m_git->listRemotes(m_repoPath)) {
            QString label = remote.name;
            if (!remote.fetchUrl.isEmpty()) {
                label += QStringLiteral(" — ") + remote.fetchUrl;
            }
            auto *item = new QListWidgetItem(label, m_remoteList);
            item->setData(Qt::UserRole, remote.name);
        }
    }

    m_remoteList->blockSignals(false);
    if (selection) {
        selection->blockSignals(false);
    }

    m_syncingList = false;
}

void RemotesDialog::enterAddMode()
{
    m_addMode = true;
    m_nameEdit->clear();
    m_nameEdit->setReadOnly(false);
    m_nameEdit->setEnabled(true);
    m_fetchUrlEdit->clear();
    m_fetchUrlEdit->setReadOnly(false);
    m_fetchUrlEdit->setEnabled(true);
    m_separatePushCheck->setChecked(false);
    m_pushUrlEdit->clear();
    m_pushUrlEdit->setEnabled(false);
    updateFormHint();

    if (m_syncingList || !m_remoteList || m_remoteList->currentRow() < 0) {
        return;
    }

    m_syncingList = true;
    QItemSelectionModel *selection = m_remoteList->selectionModel();
    if (selection) {
        selection->blockSignals(true);
    }
    m_remoteList->blockSignals(true);
    m_remoteList->clearSelection();
    m_remoteList->blockSignals(false);
    if (selection) {
        selection->blockSignals(false);
    }
    m_syncingList = false;
}

void RemotesDialog::enterEditMode(const GitRemote &remote)
{
    m_addMode = false;
    m_nameEdit->setText(remote.name);
    m_nameEdit->setReadOnly(true);
    m_nameEdit->setEnabled(true);
    m_fetchUrlEdit->setText(remote.fetchUrl);
    m_fetchUrlEdit->setReadOnly(false);
    m_fetchUrlEdit->setEnabled(true);
    if (remote.hasSeparatePushUrl()) {
        m_separatePushCheck->setChecked(true);
        m_pushUrlEdit->setText(remote.pushUrl);
        m_pushUrlEdit->setEnabled(true);
    } else {
        m_separatePushCheck->setChecked(false);
        m_pushUrlEdit->clear();
        m_pushUrlEdit->setEnabled(false);
    }
    updateFormHint();
}

void RemotesDialog::updateFormHint()
{
    if (!m_hintLabel) {
        return;
    }

    if (m_addMode) {
        m_hintLabel->setText(
            tr("Enter remote name and repository URL, then click Save."));
    } else {
        m_hintLabel->setText(tr("Editing selected remote. Click \"Add remote…\" to create another."));
    }
}

void RemotesDialog::onRemoteSelectionChanged(int row)
{
    if (m_syncingList) {
        return;
    }

    if (row < 0) {
        enterAddMode();
        return;
    }

    if (!m_git) {
        return;
    }

    const QListWidgetItem *item = m_remoteList->item(row);
    if (!item) {
        enterAddMode();
        return;
    }

    const QString name = item->data(Qt::UserRole).toString();
    for (const GitRemote &remote : m_git->listRemotes(m_repoPath)) {
        if (remote.name == name) {
            enterEditMode(remote);
            return;
        }
    }

    enterAddMode();
}

void RemotesDialog::onSeparatePushToggled(bool checked)
{
    m_pushUrlEdit->setEnabled(checked);
    if (!checked) {
        m_pushUrlEdit->clear();
    } else {
        m_pushUrlEdit->setFocus();
    }
}

GitRemote RemotesDialog::remoteFromForm() const
{
    GitRemote remote;
    remote.name = m_nameEdit->text().trimmed();
    remote.fetchUrl = m_fetchUrlEdit->text().trimmed();
    if (m_separatePushCheck->isChecked()) {
        remote.pushUrl = m_pushUrlEdit->text().trimmed();
    }
    return remote;
}

bool RemotesDialog::validateForm(QString *errorOut) const
{
    const GitRemote remote = remoteFromForm();
    if (!m_git) {
        if (errorOut) {
            *errorOut = tr("Git service is not available");
        }
        return false;
    }

    const QString nameError = m_git->validateRemoteName(remote.name);
    if (!nameError.isEmpty()) {
        if (errorOut) {
            *errorOut = nameError;
        }
        return false;
    }

    if (remote.fetchUrl.isEmpty()) {
        if (errorOut) {
            *errorOut = tr("Repository URL is empty");
        }
        return false;
    }

    if (m_separatePushCheck->isChecked() && remote.pushUrl.isEmpty()) {
        if (errorOut) {
            *errorOut = tr("Push URL is empty");
        }
        return false;
    }

    return true;
}

void RemotesDialog::onAddRemote()
{
    enterAddMode();
    m_nameEdit->setFocus();
}

void RemotesDialog::onSaveRemote()
{
    if (!m_git) {
        return;
    }

    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, tr("Remotes"), error);
        if (remoteFromForm().fetchUrl.isEmpty()) {
            m_fetchUrlEdit->setFocus();
        }
        return;
    }

    const GitRemote remote = remoteFromForm();

    if (m_addMode) {
        const GitProcessResult result = m_git->addRemote(m_repoPath, remote.name, remote.fetchUrl);
        if (!result.success()) {
            QMessageBox::critical(
                this, tr("Add remote failed"),
                tr("%1\n\n%2").arg(m_git->lastError(), result.stderrText.trimmed()));
            return;
        }

        if (remote.hasSeparatePushUrl()) {
            const GitProcessResult pushResult =
                m_git->setRemoteUrl(m_repoPath, remote.name, remote.pushUrl, true);
            if (!pushResult.success()) {
                QMessageBox::critical(
                    this, tr("Add remote failed"),
                    tr("%1\n\n%2").arg(m_git->lastError(), pushResult.stderrText.trimmed()));
                return;
            }
        }

        m_modified = true;
        reloadRemotes();
        enterAddMode();
        return;
    }

    GitProcessResult fetchResult =
        m_git->setRemoteUrl(m_repoPath, remote.name, remote.fetchUrl, false);
    if (!fetchResult.success()) {
        QMessageBox::critical(
            this, tr("Save remote failed"),
            tr("%1\n\n%2").arg(m_git->lastError(), fetchResult.stderrText.trimmed()));
        return;
    }

    if (remote.hasSeparatePushUrl()) {
        fetchResult = m_git->setRemoteUrl(m_repoPath, remote.name, remote.pushUrl, true);
    } else {
        fetchResult = m_git->setRemoteUrl(m_repoPath, remote.name, remote.fetchUrl, true);
    }

    if (!fetchResult.success()) {
        QMessageBox::critical(
            this, tr("Save remote failed"),
            tr("%1\n\n%2").arg(m_git->lastError(), fetchResult.stderrText.trimmed()));
        return;
    }

    m_modified = true;
    reloadRemotes();

    m_syncingList = true;
    for (int listRow = 0; listRow < m_remoteList->count(); ++listRow) {
        if (m_remoteList->item(listRow)->data(Qt::UserRole).toString() == remote.name) {
            m_remoteList->setCurrentRow(listRow);
            break;
        }
    }
    m_syncingList = false;
}

void RemotesDialog::onRemoveRemote()
{
    if (!m_git || m_addMode) {
        QMessageBox::information(this, tr("Remotes"), tr("Select a remote to remove."));
        return;
    }

    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        return;
    }

    const auto answer =
        QMessageBox::warning(this, tr("Remove remote"),
                             tr("Remove remote \"%1\"?\n\nThis runs: git remote remove %1")
                                 .arg(name),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const GitProcessResult result = m_git->removeRemote(m_repoPath, name);
    if (!result.success()) {
        QMessageBox::critical(
            this, tr("Remove remote failed"),
            tr("%1\n\n%2").arg(m_git->lastError(), result.stderrText.trimmed()));
        return;
    }

    m_modified = true;
    reloadRemotes();
    enterAddMode();
    m_nameEdit->setFocus();
}
