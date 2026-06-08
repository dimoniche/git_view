#include "ui/WorkingChangesPanel.h"

#include "core/WorkingTreeChange.h"
#include "git/GitService.h"
#include "ui/DiffViewerDialog.h"

#include <QAbstractItemView>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "ui/DiffHighlighter.h"

namespace {

enum TreeRoles {
    KindRole = Qt::UserRole,
    PathRole = Qt::UserRole + 1,
    ScopeRole = Qt::UserRole + 2,
};

enum class TreeItemKind { Header, File };

void makeCompactToolButton(QPushButton *button)
{
    QFont font = button->font();
    if (font.pointSize() > 0) {
        font.setPointSize(qMax(9, font.pointSize() - 1));
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(qMax(11, font.pixelSize() - 2));
    }
    button->setFont(font);
    const int height = QFontMetrics(font).height() + 6;
    button->setFixedHeight(height);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

QString statusLetter(const WorkingTreeChange &change, WorkingDiffScope section)
{
    if (change.isUntracked()) {
        return QStringLiteral("U");
    }
    if (section == WorkingDiffScope::Staged) {
        const QChar status = change.indexStatus;
        return status == QLatin1Char(' ') ? QStringLiteral("?") : QString(status);
    }
    const QChar status = change.workTreeStatus;
    return status == QLatin1Char(' ') ? QStringLiteral("?") : QString(status);
}

QString fileLineText(const WorkingTreeChange &change, WorkingDiffScope section)
{
    return QStringLiteral("%1  %2").arg(statusLetter(change, section), change.path);
}

QFont headerFont(const QFont &base)
{
    QFont font = base;
    font.setBold(true);
    return font;
}

QString fileContentText(const WorkingFileContent &content, const QString &missingLabel,
                        const QString &binaryLabel)
{
    if (content.binary) {
        return content.content.isEmpty() ? binaryLabel : content.content;
    }
    if (content.missing) {
        return missingLabel;
    }
    if (content.ok) {
        return content.content;
    }
    return content.error;
}

} // namespace

WorkingChangesPanel::WorkingChangesPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    auto *actionsRow = new QHBoxLayout();
    actionsRow->setSpacing(4);

    m_commitButton = new QPushButton(tr("Commit…"), this);
    m_commitButton->setEnabled(false);
    connect(m_commitButton, &QPushButton::clicked, this, &WorkingChangesPanel::commitRequested);
    makeCompactToolButton(m_commitButton);
    actionsRow->addWidget(m_commitButton);

    m_discardFileButton = new QPushButton(tr("Discard file…"), this);
    m_discardFileButton->setEnabled(false);
    connect(m_discardFileButton, &QPushButton::clicked, this,
            &WorkingChangesPanel::onDiscardFileClicked);
    makeCompactToolButton(m_discardFileButton);
    actionsRow->addWidget(m_discardFileButton);

    m_discardAllButton = new QPushButton(tr("Discard all…"), this);
    m_discardAllButton->setEnabled(false);
    connect(m_discardAllButton, &QPushButton::clicked, this, &WorkingChangesPanel::onDiscardAllClicked);
    makeCompactToolButton(m_discardAllButton);
    actionsRow->addWidget(m_discardAllButton);
    actionsRow->addStretch(1);
    layout->addLayout(actionsRow);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_filesTree = new QTreeWidget(splitter);
    m_filesTree->setHeaderHidden(true);
    m_filesTree->setRootIsDecorated(true);
    m_filesTree->setIndentation(14);
    m_filesTree->setMinimumHeight(80);
    m_filesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_filesTree->setUniformRowHeights(true);
    m_filesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_filesTree, &QTreeWidget::currentItemChanged, this,
            &WorkingChangesPanel::onFileSelectionChanged);
    connect(m_filesTree, &QTreeWidget::itemSelectionChanged, this,
            &WorkingChangesPanel::updateDiscardFileButton);
    connect(m_filesTree, &QTreeWidget::itemDoubleClicked, this,
            &WorkingChangesPanel::onFileDoubleClicked);
    connect(m_filesTree, &QWidget::customContextMenuRequested, this,
            &WorkingChangesPanel::showFilesContextMenu);

    splitter->addWidget(m_filesTree);

    auto *diffWidget = new QWidget(splitter);
    auto *diffLayout = new QVBoxLayout(diffWidget);
    diffLayout->setContentsMargins(0, 0, 0, 0);

    m_diffTitle = new QLabel(tr("Diff"), diffWidget);
    diffLayout->addWidget(m_diffTitle);

    m_diffView = new QPlainTextEdit(diffWidget);
    m_diffView->setReadOnly(true);
    m_diffView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diffView->setTabStopDistance(
        QFontMetrics(m_diffView->font()).horizontalAdvance(QLatin1Char(' ')) * 4);

    QFont diffFont = m_diffView->font();
    diffFont.setStyleHint(QFont::Monospace);
    diffFont.setFamily(QStringLiteral("Menlo"));
#if defined(Q_OS_WIN)
    diffFont.setFamily(QStringLiteral("Consolas"));
#elif defined(Q_OS_LINUX)
    diffFont.setFamily(QStringLiteral("Monospace"));
#endif
    m_diffView->setFont(diffFont);
    new DiffHighlighter(m_diffView->document());
    diffLayout->addWidget(m_diffView);

    splitter->addWidget(diffWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({140, 360});

    layout->addWidget(splitter, 1);

    refresh();
}

void WorkingChangesPanel::setRepoContext(const QString &repoPath, GitService *git)
{
    const QString canonical = QFileInfo(repoPath).canonicalFilePath();
    m_repoPath = canonical.isEmpty() ? repoPath : canonical;
    m_git = git;
}

void WorkingChangesPanel::setCommitEnabled(bool enabled)
{
    m_repoActionsEnabled = enabled;
    if (m_commitButton) {
        m_commitButton->setEnabled(enabled);
    }
    updateDiscardAllButton();
    updateDiscardFileButton();
}

void WorkingChangesPanel::updateDiscardAllButton()
{
    if (!m_discardAllButton) {
        return;
    }
    m_discardAllButton->setEnabled(m_repoActionsEnabled && !m_allChanges.empty());
}

bool WorkingChangesPanel::isStagedEntry(const WorkingTreeChange &change) const
{
    if (!change.hasStaged()) {
        return false;
    }
    return !m_git || m_git->hasCachedDiffForPath(m_repoPath, change.path);
}

const QTreeWidgetItem *WorkingChangesPanel::selectedFileItem() const
{
    const QTreeWidgetItem *item = m_filesTree ? m_filesTree->currentItem() : nullptr;
    if (!item) {
        return nullptr;
    }
    if (item->data(0, KindRole).toInt() != static_cast<int>(TreeItemKind::File)) {
        return nullptr;
    }
    return item;
}

QList<const QTreeWidgetItem *> WorkingChangesPanel::selectedFileItems() const
{
    QList<const QTreeWidgetItem *> items;
    if (!m_filesTree) {
        return items;
    }

    for (QTreeWidgetItem *item : m_filesTree->selectedItems()) {
        if (item && item->data(0, KindRole).toInt() == static_cast<int>(TreeItemKind::File)) {
            items.append(item);
        }
    }
    return items;
}

WorkingDiffScope WorkingChangesPanel::selectedItemScope() const
{
    const QTreeWidgetItem *item = selectedFileItem();
    if (!item) {
        return WorkingDiffScope::Unstaged;
    }
    return static_cast<WorkingDiffScope>(item->data(0, ScopeRole).toInt());
}

void WorkingChangesPanel::updateDiscardFileButton()
{
    if (!m_discardFileButton) {
        return;
    }
    const QStringList paths = selectedFilePaths();
    const bool hasFiles = !paths.isEmpty();
    m_discardFileButton->setEnabled(m_repoActionsEnabled && !m_allChanges.empty() && hasFiles);
    if (paths.size() > 1) {
        m_discardFileButton->setText(tr("Discard %1 files…").arg(paths.size()));
    } else {
        m_discardFileButton->setText(tr("Discard file…"));
    }
}

void WorkingChangesPanel::onDiscardFileClicked()
{
    const QStringList paths = selectedFilePaths();
    if (paths.isEmpty()) {
        return;
    }
    emit discardFilesRequested(paths);
}

void WorkingChangesPanel::onDiscardAllClicked()
{
    emit discardAllRequested();
}

QString WorkingChangesPanel::selectedFilePath() const
{
    const QTreeWidgetItem *item = selectedFileItem();
    if (!item) {
        return {};
    }
    return item->data(0, PathRole).toString();
}

QStringList WorkingChangesPanel::selectedFilePaths() const
{
    QStringList paths;
    for (const QTreeWidgetItem *item : selectedFileItems()) {
        const QString path = item->data(0, PathRole).toString();
        if (!path.isEmpty() && !paths.contains(path)) {
            paths.append(path);
        }
    }
    return paths;
}

bool WorkingChangesPanel::hasSelectedChange() const
{
    return !selectedFilePaths().isEmpty();
}

void WorkingChangesPanel::selectTreeItem(const QString &path, WorkingDiffScope scope)
{
    if (!m_filesTree || path.isEmpty()) {
        return;
    }

    const auto matches = [&](const QTreeWidgetItem *item) {
        return item && item->data(0, KindRole).toInt() == static_cast<int>(TreeItemKind::File)
               && item->data(0, PathRole).toString() == path
               && static_cast<WorkingDiffScope>(item->data(0, ScopeRole).toInt()) == scope;
    };

    const int topCount = m_filesTree->topLevelItemCount();
    for (int i = 0; i < topCount; ++i) {
        QTreeWidgetItem *section = m_filesTree->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *child = section->child(j);
            if (matches(child)) {
                m_filesTree->setCurrentItem(child);
                return;
            }
        }
    }
}

void WorkingChangesPanel::showFilesContextMenu(const QPoint &pos)
{
    QMenu menu(this);

    QTreeWidgetItem *item = m_filesTree->itemAt(pos);
    const bool isFile =
        item && item->data(0, KindRole).toInt() == static_cast<int>(TreeItemKind::File);

    if (isFile) {
        if (!item->isSelected()) {
            m_filesTree->clearSelection();
            item->setSelected(true);
        }
        m_filesTree->setCurrentItem(item);

        const QStringList selectedPaths = selectedFilePaths();
        const QString path = item->data(0, PathRole).toString();
        const WorkingTreeChange change = changeForPath(path);

        QAction *discardFileAction = menu.addAction(
            selectedPaths.size() > 1
                ? tr("Discard changes to %1 files…").arg(selectedPaths.size())
                : tr("Discard file changes…"),
            this, &WorkingChangesPanel::onDiscardFileClicked);
        discardFileAction->setEnabled(m_discardFileButton && m_discardFileButton->isEnabled());

        const bool inStaged = isStagedEntry(change);
        const bool inChanges = change.hasUnstaged() || change.isUntracked();
        if (inStaged && inChanges) {
            QMenu *diffMenu = menu.addMenu(tr("Show diff"));
            diffMenu->addAction(tr("Staged Changes"), this, [this, path]() {
                selectTreeItem(path, WorkingDiffScope::Staged);
            });
            diffMenu->addAction(tr("Changes"), this, [this, path]() {
                selectTreeItem(path, WorkingDiffScope::Unstaged);
            });
            diffMenu->addAction(tr("All vs HEAD"), this, [this, path]() {
                if (!m_git) {
                    return;
                }
                const WorkingTreeChange ch = changeForPath(path);
                const QString diff =
                    m_git->workingTreeFileDiff(m_repoPath, path, WorkingDiffScope::AgainstHead, ch);
                showDiffText(diff.isEmpty() ? tr("No diff vs HEAD.") : diff,
                             tr("Diff (vs HEAD): %1").arg(path));
            });
        } else if (inStaged || inChanges) {
            menu.addAction(tr("Diff vs HEAD"), this, [this, path]() {
                if (!m_git) {
                    return;
                }
                const WorkingTreeChange ch = changeForPath(path);
                const QString diff =
                    m_git->workingTreeFileDiff(m_repoPath, path, WorkingDiffScope::AgainstHead, ch);
                showDiffText(diff.isEmpty() ? tr("No diff vs HEAD.") : diff,
                             tr("Diff (vs HEAD): %1").arg(path));
            });
        }

        menu.addAction(tr("Add to .gitignore"), this, [this, path]() {
            emit addToGitignoreRequested(path);
        });

        menu.addSeparator();
    }

    QAction *commitAction =
        menu.addAction(tr("Commit changes…"), this, [this]() { emit commitRequested(); });
    commitAction->setEnabled(m_commitButton && m_commitButton->isEnabled());

    QAction *discardAllAction =
        menu.addAction(tr("Discard all changes…"), this, &WorkingChangesPanel::onDiscardAllClicked);
    discardAllAction->setEnabled(m_discardAllButton && m_discardAllButton->isEnabled());

    menu.exec(m_filesTree->mapToGlobal(pos));
}

void WorkingChangesPanel::refresh()
{
    m_allChanges.clear();
    m_filesTree->clear();

    if (!m_git || m_repoPath.isEmpty()) {
        m_summaryLabel->setText(tr("Open a repository"));
        showDiffText({}, tr("Diff"));
        updateDiscardAllButton();
        updateDiscardFileButton();
        return;
    }

    m_allChanges = m_git->workingTreeChanges(m_repoPath);
    if (m_allChanges.empty() && !m_git->lastError().isEmpty()) {
        m_summaryLabel->setText(m_git->lastError());
        showDiffText({}, tr("Diff"));
        updateDiscardAllButton();
        updateDiscardFileButton();
        return;
    }

    if (m_allChanges.empty()) {
        m_summaryLabel->setText(tr("Working tree clean — no uncommitted changes"));
        auto *empty = new QTreeWidgetItem(m_filesTree);
        empty->setText(0, tr("(no changes)"));
        empty->setFlags(Qt::NoItemFlags);
        showDiffText({}, tr("Diff"));
        updateDiscardAllButton();
        updateDiscardFileButton();
        return;
    }

    rebuildChangeTree();
}

WorkingTreeChange WorkingChangesPanel::changeForPath(const QString &path) const
{
    for (const WorkingTreeChange &change : m_allChanges) {
        if (change.path == path) {
            return change;
        }
    }

    if (m_git && !m_repoPath.isEmpty()) {
        return m_git->changeForPath(m_repoPath, path);
    }

    WorkingTreeChange fallback;
    fallback.path = path;
    return fallback;
}

void WorkingChangesPanel::rebuildChangeTree()
{
    const QString previousPath = selectedFilePath();
    const WorkingDiffScope previousScope = selectedItemScope();

    std::vector<WorkingTreeChange> stagedFiles;
    std::vector<WorkingTreeChange> changeFiles;

    for (const WorkingTreeChange &change : m_allChanges) {
        if (isStagedEntry(change)) {
            stagedFiles.push_back(change);
        }
        if (change.hasUnstaged() || change.isUntracked()) {
            changeFiles.push_back(change);
        }
    }

    QSignalBlocker treeBlocker(m_filesTree);
    m_filesTree->clear();

    const QFont sectionFont = headerFont(m_filesTree->font());

    auto addSection = [&](const QString &title, const std::vector<WorkingTreeChange> &files,
                          WorkingDiffScope scope) -> QTreeWidgetItem * {
        if (files.empty()) {
            return nullptr;
        }
        auto *section = new QTreeWidgetItem(m_filesTree);
        section->setText(0, title);
        section->setFont(0, sectionFont);
        section->setData(0, KindRole, static_cast<int>(TreeItemKind::Header));
        section->setFlags(Qt::ItemIsEnabled);
        section->setExpanded(true);

        for (const WorkingTreeChange &change : files) {
            auto *fileItem = new QTreeWidgetItem(section);
            fileItem->setText(0, fileLineText(change, scope));
            fileItem->setData(0, KindRole, static_cast<int>(TreeItemKind::File));
            fileItem->setData(0, PathRole, change.path);
            fileItem->setData(0, ScopeRole, static_cast<int>(scope));
            fileItem->setToolTip(
                0, tr("Porcelain %1 — %2").arg(change.porcelainStatusDisplay(), change.path));
            fileItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        }
        return section;
    };

    addSection(tr("Staged Changes (%1)").arg(stagedFiles.size()), stagedFiles,
               WorkingDiffScope::Staged);
    addSection(tr("Changes (%1)").arg(changeFiles.size()), changeFiles, WorkingDiffScope::Unstaged);

    if (stagedFiles.empty() && changeFiles.empty()) {
        auto *empty = new QTreeWidgetItem(m_filesTree);
        empty->setText(0, tr("(no changes)"));
        empty->setFlags(Qt::NoItemFlags);
        m_summaryLabel->setText(tr("Working tree clean"));
        showDiffText({}, tr("Diff"));
        updateDiscardAllButton();
        updateDiscardFileButton();
        return;
    }

    m_summaryLabel->setText(tr("%1 staged, %2 changes")
                                .arg(stagedFiles.size())
                                .arg(changeFiles.size()));

    QTreeWidgetItem *selectItem = nullptr;
    const int topCount = m_filesTree->topLevelItemCount();
    for (int i = 0; i < topCount; ++i) {
        QTreeWidgetItem *section = m_filesTree->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *child = section->child(j);
            if (child->data(0, KindRole).toInt() != static_cast<int>(TreeItemKind::File)) {
                continue;
            }
            if (!previousPath.isEmpty() && child->data(0, PathRole).toString() == previousPath
                && static_cast<WorkingDiffScope>(child->data(0, ScopeRole).toInt())
                       == previousScope) {
                selectItem = child;
                break;
            }
        }
        if (selectItem) {
            break;
        }
    }

    if (!selectItem) {
        for (int i = 0; i < topCount; ++i) {
            QTreeWidgetItem *section = m_filesTree->topLevelItem(i);
            if (section->childCount() > 0) {
                selectItem = section->child(0);
                break;
            }
        }
    }

    if (selectItem) {
        m_filesTree->setCurrentItem(selectItem);
    }

    updateDiscardAllButton();
    updateDiscardFileButton();

    if (selectedFileItem()) {
        loadDiffForCurrentFile();
    }
}

void WorkingChangesPanel::onFileSelectionChanged()
{
    loadDiffForCurrentFile();
    updateDiscardFileButton();
    emit fileSelectionChanged();
}

void WorkingChangesPanel::onFileDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item || item->data(0, KindRole).toInt() != static_cast<int>(TreeItemKind::File)) {
        return;
    }
    m_filesTree->setCurrentItem(item);
    openDiffInSeparateWindow();
}

void WorkingChangesPanel::loadDiffForCurrentFile()
{
    if (!m_git || m_repoPath.isEmpty()) {
        showDiffText(tr("Open a repository."), tr("Diff"));
        return;
    }

    const QTreeWidgetItem *item = selectedFileItem();
    if (!item) {
        showDiffText({}, tr("Diff"));
        return;
    }

    const QString path = item->data(0, PathRole).toString();
    if (path.isEmpty()) {
        showDiffText({}, tr("Diff"));
        return;
    }

    const WorkingTreeChange change = changeForPath(path);
    const WorkingDiffScope scope = selectedItemScope();

    QString scopeLabel;
    QString sectionLabel;
    switch (scope) {
    case WorkingDiffScope::Staged:
        scopeLabel = tr("staged");
        sectionLabel = tr("Staged Changes");
        break;
    case WorkingDiffScope::AgainstHead:
        scopeLabel = tr("vs HEAD");
        sectionLabel = tr("All vs HEAD");
        break;
    case WorkingDiffScope::Unstaged:
        scopeLabel = tr("changes");
        sectionLabel = tr("Changes");
        break;
    }

    m_diffTitle->setText(tr("%1 — %2").arg(sectionLabel, path));

    const QString diff = m_git->workingTreeFileDiff(m_repoPath, path, scope, change);

    if (diff.isEmpty() && !m_git->lastError().isEmpty()) {
        showDiffText(m_git->lastError(), m_diffTitle->text());
        return;
    }

    if (diff.isEmpty()) {
        const bool cachedStaged =
            m_git && m_git->hasCachedDiffForPath(m_repoPath, path);
        QString hint = tr("No diff for \"%1\".").arg(scopeLabel);
        if (scope == WorkingDiffScope::Staged && !cachedStaged) {
            if (!change.hasStaged() && change.hasUnstaged()) {
                hint = tr("This file has only unstaged changes. See the Changes section.");
            } else if (change.hasStaged() && !change.hasUnstaged()) {
                hint = tr("Git status shows staged, but there is no staged diff vs HEAD. "
                          "Try: git add \"%1\"")
                           .arg(path);
            } else {
                hint = tr("There is no staged diff for this file in the index.");
            }
        } else if (scope == WorkingDiffScope::Staged && cachedStaged) {
            hint = tr("Staged diff exists but could not be loaded.");
            if (!m_git->lastDiffCommand().isEmpty()) {
                hint += QLatin1Char('\n') + tr("Try in terminal: %1").arg(m_git->lastDiffCommand());
            }
        }
        hint += QLatin1Char('\n')
                + tr("Porcelain %1 (%2) — %3")
                      .arg(change.porcelainStatusDisplay(), change.statusDescription(), path);
        const QString cmd = m_git->lastDiffCommand();
        if (!cmd.isEmpty()) {
            hint += QLatin1Char('\n') + tr("Last command: %1").arg(cmd);
        }
        if (!m_git->lastError().isEmpty()) {
            hint += QLatin1Char('\n') + m_git->lastError();
        }
        showDiffText(hint, m_diffTitle->text());
        return;
    }

    showDiffText(diff, m_diffTitle->text());
}

void WorkingChangesPanel::openDiffInSeparateWindow()
{
    if (!m_git || m_repoPath.isEmpty()) {
        return;
    }

    const QTreeWidgetItem *item = selectedFileItem();
    if (!item) {
        return;
    }

    const QString path = item->data(0, PathRole).toString();
    if (path.isEmpty()) {
        return;
    }

    const WorkingTreeChange change = changeForPath(path);
    const WorkingDiffScope scope = selectedItemScope();

    QString sectionLabel;
    switch (scope) {
    case WorkingDiffScope::Staged:
        sectionLabel = tr("Staged Changes");
        break;
    case WorkingDiffScope::AgainstHead:
        sectionLabel = tr("All vs HEAD");
        break;
    case WorkingDiffScope::Unstaged:
        sectionLabel = tr("Changes");
        break;
    }

    const QString title = tr("%1 — %2").arg(sectionLabel, path);
    const QString diff = m_git->workingTreeFileDiff(m_repoPath, path, scope, change);

    if (diff.isEmpty() && !m_git->lastError().isEmpty()) {
        DiffViewerDialog::showDiff(this, title, m_git->lastError());
        return;
    }

    if (diff.isEmpty()) {
        const bool cachedStaged = m_git->hasCachedDiffForPath(m_repoPath, path);
        QString scopeLabel;
        switch (scope) {
        case WorkingDiffScope::Staged:
            scopeLabel = tr("staged");
            break;
        case WorkingDiffScope::AgainstHead:
            scopeLabel = tr("vs HEAD");
            break;
        case WorkingDiffScope::Unstaged:
            scopeLabel = tr("changes");
            break;
        }

        QString hint = tr("No diff for \"%1\".").arg(scopeLabel);
        if (scope == WorkingDiffScope::Staged && !cachedStaged) {
            if (!change.hasStaged() && change.hasUnstaged()) {
                hint = tr("This file has only unstaged changes. See the Changes section.");
            } else if (change.hasStaged() && !change.hasUnstaged()) {
                hint = tr("Git status shows staged, but there is no staged diff vs HEAD. "
                          "Try: git add \"%1\"")
                           .arg(path);
            } else {
                hint = tr("There is no staged diff for this file in the index.");
            }
        } else if (scope == WorkingDiffScope::Staged && cachedStaged) {
            hint = tr("Staged diff exists but could not be loaded.");
            if (!m_git->lastDiffCommand().isEmpty()) {
                hint += QLatin1Char('\n') + tr("Try in terminal: %1").arg(m_git->lastDiffCommand());
            }
        }
        hint += QLatin1Char('\n')
                + tr("Porcelain %1 (%2) — %3")
                      .arg(change.porcelainStatusDisplay(), change.statusDescription(), path);
        const QString cmd = m_git->lastDiffCommand();
        if (!cmd.isEmpty()) {
            hint += QLatin1Char('\n') + tr("Last command: %1").arg(cmd);
        }
        if (!m_git->lastError().isEmpty()) {
            hint += QLatin1Char('\n') + m_git->lastError();
        }
        DiffViewerDialog::showDiff(this, title, hint);
        return;
    }

    const DiffViewerSources sources = buildSourcesForFile(path, scope, change);
    DiffViewerDialog::showDiff(this, title, diff, sources);
}

DiffViewerSources WorkingChangesPanel::buildSourcesForFile(const QString &path,
                                                           WorkingDiffScope scope,
                                                           const WorkingTreeChange &change) const
{
    DiffViewerSources sources;
    sources.beforeCaption = tr("Before");
    sources.afterCaption = tr("After");

    if (!m_git || m_repoPath.isEmpty()) {
        return sources;
    }

    const WorkingFileContent before =
        m_git->workingTreeFileContent(m_repoPath, path, scope, change, WorkingFileSide::Before);
    const WorkingFileContent after =
        m_git->workingTreeFileContent(m_repoPath, path, scope, change, WorkingFileSide::After);

    sources.before = fileContentText(
        before, tr("(no previous version)"), tr("(binary file — cannot display as text)"));
    sources.after = fileContentText(after, tr("(file not on disk)"),
                                    tr("(binary file — cannot display as text)"));

    return sources;
}

void WorkingChangesPanel::showDiffText(const QString &text, const QString &title)
{
    if (!title.isEmpty()) {
        m_diffTitle->setText(title);
    }
    m_diffView->setPlainText(text);
}
