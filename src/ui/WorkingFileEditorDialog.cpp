#include "ui/WorkingFileEditorDialog.h"

#include "ui/DialogTitleBar.h"
#include "ui/SourceChangeHighlighter.h"
#include "ui/SourceCodeView.h"
#include "ui/TopLevelDialogUtils.h"

#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSizeGrip>
#include <QVBoxLayout>

namespace {

struct EditorLoadResult {
    bool ok = false;
    bool binary = false;
    QString content;
    QString error;
};

EditorLoadResult loadEditableContent(GitService *git, const QString &repoPath, const QString &path,
                                     const WorkingTreeChange &change, WorkingDiffScope scope)
{
    EditorLoadResult result;
    if (!git || repoPath.isEmpty() || path.isEmpty()) {
        result.error = WorkingFileEditorDialog::tr("Repository or file path is missing.");
        return result;
    }

    const auto useContent = [&](const WorkingFileContent &file) -> bool {
        if (file.binary) {
            result.binary = true;
            return true;
        }
        if (file.missing) {
            result.error = WorkingFileEditorDialog::tr("File is not available for editing.");
            return true;
        }
        if (file.ok) {
            result.ok = true;
            result.content = file.content;
            return true;
        }
        result.error = file.error.isEmpty() ? WorkingFileEditorDialog::tr("Could not load file.")
                                            : file.error;
        return true;
    };

    const bool stagedOnly =
        scope == WorkingDiffScope::Staged && change.hasStaged() && !change.hasUnstaged()
        && !change.isUntracked();

    if (stagedOnly) {
        const WorkingFileContent indexContent =
            git->workingTreeFileContent(repoPath, path, WorkingDiffScope::Staged, change,
                                        WorkingFileSide::After);
        useContent(indexContent);
        return result;
    }

    const WorkingFileContent worktree =
        git->workingTreeFileContent(repoPath, path, WorkingDiffScope::Unstaged, change,
                                    WorkingFileSide::After);
    if (worktree.binary) {
        result.binary = true;
        return result;
    }
    if (worktree.ok) {
        result.ok = true;
        result.content = worktree.content;
        return result;
    }
    if (!worktree.missing && !worktree.error.isEmpty()) {
        result.error = worktree.error;
        return result;
    }

    if (change.hasStaged()) {
        const WorkingFileContent indexContent =
            git->workingTreeFileContent(repoPath, path, WorkingDiffScope::Staged, change,
                                        WorkingFileSide::After);
        useContent(indexContent);
    }

    return result;
}

QFont editorMonospaceFont(const QFont &base)
{
    QFont font = base;
    font.setStyleHint(QFont::Monospace);
    font.setFamily(QStringLiteral("Menlo"));
#if defined(Q_OS_WIN)
    font.setFamily(QStringLiteral("Consolas"));
#elif defined(Q_OS_LINUX)
    font.setFamily(QStringLiteral("Monospace"));
#endif
    return font;
}

} // namespace

WorkingFileEditorDialog::WorkingFileEditorDialog(GitService *git, const QString &repoPath,
                                                   const QString &relativePath,
                                                   const WorkingTreeChange &change,
                                                   WorkingDiffScope scope, SavedCallback onSaved,
                                                   QWidget *parent)
    : QWidget(parent)
    , m_git(git)
    , m_repoPath(repoPath)
    , m_relativePath(relativePath)
    , m_change(change)
    , m_scope(scope)
    , m_onSaved(std::move(onSaved))
{
    const QString title = tr("Edit — %1").arg(relativePath);
    setWindowTitle(title);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(420, 280);
    resize(900, 640);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(new DialogTitleBar(title, this));

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    auto *toolbar = new QHBoxLayout();
    m_saveButton = new QPushButton(tr("Save"), content);
    m_saveButton->setEnabled(false);
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (saveToDisk()) {
            updateSaveState();
        }
    });
    toolbar->addWidget(m_saveButton);
    toolbar->addStretch();
    m_statusLabel = new QLabel(content);
    toolbar->addWidget(m_statusLabel);
    contentLayout->addLayout(toolbar);

    m_editor = new SourceCodeView(content);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setTabStopDistance(
        QFontMetrics(m_editor->font()).horizontalAdvance(QLatin1Char(' ')) * 4);
    m_editor->setFont(editorMonospaceFont(m_editor->font()));
    auto *highlighter =
        new SourceChangeHighlighter(SourceChangeHighlighter::Kind::Added, m_editor,
                                    m_editor->document());
    highlighter->setFilePath(relativePath);
    contentLayout->addWidget(m_editor, 1);

    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
        m_dirty = m_editor->toPlainText() != m_loadedContent;
        updateSaveState();
    });

    auto *gripRow = new QHBoxLayout();
    gripRow->addStretch();
    gripRow->addWidget(new QSizeGrip(content));
    contentLayout->addLayout(gripRow);

    layout->addWidget(content, 1);

    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    saveShortcut->setContext(Qt::WindowShortcut);
    connect(saveShortcut, &QShortcut::activated, this, [this]() {
        if (saveToDisk()) {
            updateSaveState();
        }
    });

    auto *closeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    closeShortcut->setContext(Qt::WindowShortcut);
    connect(closeShortcut, &QShortcut::activated, this, &QWidget::close);

    if (!loadContent()) {
        m_editor->setEnabled(false);
        m_saveButton->setEnabled(false);
    }
}

void WorkingFileEditorDialog::open(QWidget *parent, GitService *git, const QString &repoPath,
                                   const QString &relativePath, const WorkingTreeChange &change,
                                   WorkingDiffScope scope, SavedCallback onSaved)
{
    auto *dialog = new WorkingFileEditorDialog(git, repoPath, relativePath, change, scope,
                                               std::move(onSaved), nullptr);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connectRestoreHiddenOwner(dialog, parent);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

QString WorkingFileEditorDialog::absolutePath() const
{
    return QDir(m_repoPath).filePath(m_relativePath);
}

bool WorkingFileEditorDialog::loadContent()
{
    const EditorLoadResult loaded =
        loadEditableContent(m_git, m_repoPath, m_relativePath, m_change, m_scope);

    if (loaded.binary) {
        m_statusLabel->setText(tr("Binary file — editing is not supported."));
        QMessageBox::information(this, tr("Edit file"),
                                 tr("Binary files cannot be edited in git_view."));
        return false;
    }

    if (!loaded.ok) {
        m_statusLabel->setText(loaded.error);
        QMessageBox::warning(this, tr("Edit file"), loaded.error);
        return false;
    }

    m_loadedContent = loaded.content;
    m_editor->setPlainText(m_loadedContent);
    m_dirty = false;
    updateSaveState();
    return true;
}

bool WorkingFileEditorDialog::saveToDisk()
{
    if (!m_editor || !m_editor->isEnabled()) {
        return false;
    }

    const QString content = m_editor->toPlainText();
    const QString absPath = absolutePath();
    const QFileInfo info(absPath);
    const QDir parentDir = info.dir();
    if (!parentDir.exists() && !parentDir.mkpath(QStringLiteral("."))) {
        QMessageBox::warning(this, tr("Save file"),
                             tr("Could not create directory \"%1\".").arg(parentDir.path()));
        return false;
    }

    QFile file(absPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Save file"),
                             tr("Could not write \"%1\": %2").arg(absPath, file.errorString()));
        return false;
    }

    if (file.write(content.toUtf8()) < 0) {
        QMessageBox::warning(this, tr("Save file"),
                             tr("Could not write \"%1\": %2").arg(absPath, file.errorString()));
        return false;
    }

    m_loadedContent = content;
    m_dirty = false;
    m_statusLabel->setText(tr("Saved."));
    if (m_onSaved) {
        m_onSaved();
    }
    return true;
}

bool WorkingFileEditorDialog::confirmDiscard()
{
    if (!m_dirty) {
        return true;
    }

    const QMessageBox::StandardButton choice =
        QMessageBox::question(this, tr("Unsaved changes"),
                              tr("Save changes to \"%1\" before closing?").arg(m_relativePath),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                              QMessageBox::Save);
    if (choice == QMessageBox::Cancel) {
        return false;
    }
    if (choice == QMessageBox::Save) {
        return saveToDisk();
    }
    return true;
}

void WorkingFileEditorDialog::updateSaveState()
{
    if (m_saveButton) {
        m_saveButton->setEnabled(m_editor && m_editor->isEnabled() && m_dirty);
    }
    if (m_statusLabel && !m_dirty && m_editor && m_editor->isEnabled()) {
        m_statusLabel->setText({});
    } else if (m_statusLabel && m_dirty) {
        m_statusLabel->setText(tr("Modified"));
    }
}

void WorkingFileEditorDialog::closeEvent(QCloseEvent *event)
{
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}
