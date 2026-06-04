#include "ui/WorkingChangesPanel.h"

#include "core/WorkingTreeChange.h"
#include "git/GitService.h"
#include "ui/DiffHighlighter.h"

#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

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

    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->addItem(tr("Unstaged changes"), static_cast<int>(WorkingDiffScope::Unstaged));
    m_scopeCombo->addItem(tr("Staged changes"), static_cast<int>(WorkingDiffScope::Staged));
    m_scopeCombo->addItem(tr("All vs HEAD"), static_cast<int>(WorkingDiffScope::AgainstHead));
    connect(m_scopeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &WorkingChangesPanel::onScopeChanged);
    layout->addWidget(m_scopeCombo);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    auto *filesWidget = new QWidget(splitter);
    auto *filesLayout = new QVBoxLayout(filesWidget);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->addWidget(new QLabel(tr("Changed files"), filesWidget));

    m_filesList = new QListWidget(filesWidget);
    m_filesList->setMinimumHeight(80);
    m_filesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_filesList, &QListWidget::currentRowChanged, this,
            &WorkingChangesPanel::onFileSelectionChanged);
    connect(m_filesList, &QWidget::customContextMenuRequested, this,
            &WorkingChangesPanel::showFilesContextMenu);
    filesLayout->addWidget(m_filesList);

    splitter->addWidget(filesWidget);

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
    if (m_commitButton) {
        m_commitButton->setEnabled(enabled);
    }
    if (m_discardAllButton) {
        m_discardAllButton->setEnabled(enabled);
    }
    updateDiscardFileButton();
}

void WorkingChangesPanel::updateDiscardFileButton()
{
    if (!m_discardFileButton) {
        return;
    }
    const bool hasRepo = m_git && !m_repoPath.isEmpty();
    const int row = m_filesList ? m_filesList->currentRow() : -1;
    const bool hasFile =
        hasRepo && row >= 0 && row < static_cast<int>(m_changes.size()) && !m_changes.empty();
    m_discardFileButton->setEnabled(hasFile && m_discardAllButton && m_discardAllButton->isEnabled());
}

void WorkingChangesPanel::onDiscardFileClicked()
{
    const int row = m_filesList->currentRow();
    if (row < 0 || row >= static_cast<int>(m_changes.size())) {
        return;
    }
    emit discardFileRequested(m_changes[static_cast<size_t>(row)].path);
}

void WorkingChangesPanel::onDiscardAllClicked()
{
    emit discardAllRequested();
}

QString WorkingChangesPanel::selectedFilePath() const
{
    const int row = m_filesList ? m_filesList->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(m_changes.size())) {
        return {};
    }
    return m_changes[static_cast<size_t>(row)].path;
}

bool WorkingChangesPanel::hasSelectedChange() const
{
    const int row = m_filesList ? m_filesList->currentRow() : -1;
    return row >= 0 && row < static_cast<int>(m_changes.size()) && !m_changes.empty();
}

void WorkingChangesPanel::showFilesContextMenu(const QPoint &pos)
{
    QMenu menu(this);

    QListWidgetItem *item = m_filesList->itemAt(pos);
    const int row = item ? m_filesList->row(item) : -1;
    const bool hasChange =
        row >= 0 && row < static_cast<int>(m_changes.size()) && !m_changes.empty();

    if (hasChange) {
        m_filesList->setCurrentRow(row);
        const WorkingTreeChange &change = m_changes[static_cast<size_t>(row)];

        QAction *discardFileAction =
            menu.addAction(tr("Discard file changes…"), this, &WorkingChangesPanel::onDiscardFileClicked);
        discardFileAction->setEnabled(m_discardFileButton && m_discardFileButton->isEnabled());

        QMenu *diffMenu = menu.addMenu(tr("Show diff"));
        if (change.hasUnstaged() || change.isUntracked()) {
            diffMenu->addAction(tr("Unstaged changes"), this, [this]() {
                m_scopeCombo->setCurrentIndex(0);
                loadDiffForCurrentFile();
            });
        }
        if (change.hasStaged()) {
            diffMenu->addAction(tr("Staged changes"), this, [this]() {
                m_scopeCombo->setCurrentIndex(1);
                loadDiffForCurrentFile();
            });
        }
        diffMenu->addAction(tr("All vs HEAD"), this, [this]() {
            m_scopeCombo->setCurrentIndex(2);
            loadDiffForCurrentFile();
        });

        menu.addSeparator();
    }

    QAction *commitAction =
        menu.addAction(tr("Commit changes…"), this, [this]() { emit commitRequested(); });
    commitAction->setEnabled(m_commitButton && m_commitButton->isEnabled());

    QAction *discardAllAction =
        menu.addAction(tr("Discard all changes…"), this, &WorkingChangesPanel::onDiscardAllClicked);
    discardAllAction->setEnabled(m_discardAllButton && m_discardAllButton->isEnabled());

    menu.exec(m_filesList->mapToGlobal(pos));
}

void WorkingChangesPanel::refresh()
{
    m_filesList->clear();

    if (!m_git || m_repoPath.isEmpty()) {
        m_summaryLabel->setText(tr("Open a repository"));
        showDiffText({}, tr("Diff"));
        updateDiscardFileButton();
        return;
    }

    m_changes = m_git->workingTreeChanges(m_repoPath);
    if (m_changes.empty() && !m_git->lastError().isEmpty()) {
        m_summaryLabel->setText(m_git->lastError());
        showDiffText({}, tr("Diff"));
        return;
    }

    int stagedCount = 0;
    int unstagedCount = 0;
    for (const WorkingTreeChange &change : m_changes) {
        if (change.hasStaged()) {
            ++stagedCount;
        }
        if (change.hasUnstaged()) {
            ++unstagedCount;
        }

        auto *item = new QListWidgetItem(
            QStringLiteral("[%1] %2").arg(change.statusDescription(), change.path), m_filesList);
        item->setData(Qt::UserRole, change.path);
        item->setData(Qt::UserRole + 1, change.statusLabel());
        item->setToolTip(tr("Status %1 — %2").arg(change.statusLabel(), change.path));
    }

    if (m_changes.empty()) {
        m_summaryLabel->setText(tr("Working tree clean — no uncommitted changes"));
        m_filesList->addItem(tr("(no changes)"));
        showDiffText({}, tr("Diff"));
        updateDiscardFileButton();
        return;
    }

    m_summaryLabel->setText(tr("%1 file(s): %2 staged, %3 unstaged")
                                .arg(m_changes.size())
                                .arg(stagedCount)
                                .arg(unstagedCount));

    m_filesList->setCurrentRow(0);
    updateDiscardFileButton();
    emit fileSelectionChanged();
}

void WorkingChangesPanel::applyBestScopeForChange(const WorkingTreeChange &change)
{
    QSignalBlocker blocker(m_scopeCombo);
    if (change.isUntracked()) {
        m_scopeCombo->setCurrentIndex(0);
    } else if (change.hasStaged() && change.hasUnstaged()) {
        m_scopeCombo->setCurrentIndex(2);
    } else if (change.hasStaged()) {
        m_scopeCombo->setCurrentIndex(1);
    } else {
        m_scopeCombo->setCurrentIndex(0);
    }
}

void WorkingChangesPanel::onFileSelectionChanged()
{
    const int row = m_filesList->currentRow();
    if (row >= 0 && row < static_cast<int>(m_changes.size())) {
        applyBestScopeForChange(m_changes[static_cast<size_t>(row)]);
    }
    loadDiffForCurrentFile();
    updateDiscardFileButton();
    emit fileSelectionChanged();
}

void WorkingChangesPanel::onScopeChanged()
{
    loadDiffForCurrentFile();
}

WorkingDiffScope WorkingChangesPanel::currentScope() const
{
    switch (m_scopeCombo->currentIndex()) {
    case 1:
        return WorkingDiffScope::Staged;
    case 2:
        return WorkingDiffScope::AgainstHead;
    default:
        return WorkingDiffScope::Unstaged;
    }
}

void WorkingChangesPanel::loadDiffForCurrentFile()
{
    if (!m_git || m_repoPath.isEmpty()) {
        showDiffText(tr("Open a repository."), tr("Diff"));
        return;
    }

    const QListWidgetItem *item = m_filesList->currentItem();
    if (!item) {
        showDiffText({}, tr("Diff"));
        return;
    }

    m_changes = m_git->workingTreeChanges(m_repoPath);

    WorkingTreeChange change;
    QString path;

    const int row = m_filesList->currentRow();
    if (row >= 0 && row < static_cast<int>(m_changes.size())) {
        change = m_changes[static_cast<size_t>(row)];
        path = change.path;
    } else {
        path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) {
            showDiffText({}, tr("Diff"));
            return;
        }
        for (const WorkingTreeChange &candidate : m_changes) {
            if (candidate.path == path) {
                change = candidate;
                path = change.path;
                break;
            }
        }
        if (change.path.isEmpty()) {
            change.path = path;
            const QString xy = item->data(Qt::UserRole + 1).toString();
            if (xy.size() >= 2) {
                change.indexStatus = xy.at(0);
                change.workTreeStatus = xy.at(1);
            }
        }
    }

    if (path.isEmpty()) {
        showDiffText({}, tr("Diff"));
        return;
    }

    WorkingDiffScope scope = currentScope();
    QString scopeLabel;
    switch (scope) {
    case WorkingDiffScope::Staged:
        scopeLabel = tr("staged");
        break;
    case WorkingDiffScope::AgainstHead:
        scopeLabel = tr("vs HEAD");
        break;
    case WorkingDiffScope::Unstaged:
        scopeLabel = tr("unstaged");
        break;
    }

    m_diffTitle->setText(tr("Diff (%1): %2").arg(scopeLabel, path));

    QString diff = m_git->workingTreeFileDiff(m_repoPath, path, scope, change);

    if (diff.isEmpty() && scope == WorkingDiffScope::Staged && change.hasUnstaged()
        && !change.hasStaged()) {
        scope = WorkingDiffScope::Unstaged;
        scopeLabel = tr("unstaged");
        m_diffTitle->setText(tr("Diff (%1): %2 — %3")
                                 .arg(scopeLabel, path, tr("no staged changes")));
        diff = m_git->workingTreeFileDiff(m_repoPath, path, scope, change);
    }

    if (diff.isEmpty() && !m_git->lastError().isEmpty()) {
        showDiffText(m_git->lastError(), m_diffTitle->text());
        return;
    }

    if (diff.isEmpty() && scope == WorkingDiffScope::Staged && change.hasStaged()) {
        diff = m_git->workingTreeFileDiff(m_repoPath, path, WorkingDiffScope::AgainstHead, change);
        if (!diff.isEmpty()) {
            scopeLabel = tr("vs HEAD");
            m_diffTitle->setText(tr("Diff (%1): %2").arg(scopeLabel, path));
        }
    }

    if (diff.isEmpty()) {
        QString hint = tr("No diff for \"%1\".").arg(scopeLabel);
        if (!change.hasStaged() && change.hasUnstaged()) {
            hint = tr("This file has only unstaged changes. Use \"Unstaged changes\" or \"All vs HEAD\".");
        } else if (change.hasStaged() && !change.hasUnstaged()
                   && scope == WorkingDiffScope::Unstaged) {
            hint = tr("This file has only staged changes. Use \"Staged changes\" or \"All vs HEAD\".");
        } else if (change.hasStaged() && change.hasUnstaged()) {
            hint += QLatin1Char(' ') + tr("Try another scope from the list above.");
        } else if (change.hasStaged() && scope == WorkingDiffScope::Staged) {
            hint = tr("Staged changes are recorded but git returned an empty patch.");
        }
        hint += QLatin1Char('\n') + tr("Git status: %1 — %2")
                    .arg(change.statusLabel(), path);
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

void WorkingChangesPanel::showDiffText(const QString &text, const QString &title)
{
    if (!title.isEmpty()) {
        m_diffTitle->setText(title);
    }
    m_diffView->setPlainText(text);
}
