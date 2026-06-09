#include "ui/RepoTerminalPanel.h"

#include "ui/VtermTerminalWidget.h"

#include <QFileInfo>
#include <QLabel>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#if !defined(Q_OS_UNIX)
#include <QPlainTextEdit>
#endif

namespace {

QString normalizePath(const QString &path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? path : canonical;
}

} // namespace

RepoTerminalPanel::RepoTerminalPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_cwdLabel = new QLabel(this);
    m_cwdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_cwdLabel);

#if defined(Q_OS_UNIX)
    m_terminal = new VtermTerminalWidget(this);
    connect(m_terminal, &VtermTerminalWidget::shellExited, this,
            &RepoTerminalPanel::onShellExited);
    layout->addWidget(m_terminal, 1);
    m_cwdLabel->setText(tr("Interactive terminal (PTY)"));
#else
    auto *fallback = new QPlainTextEdit(this);
    fallback->setReadOnly(true);
    fallback->setPlainText(tr("Interactive PTY terminal is available on Linux and macOS only."));
    layout->addWidget(fallback, 1);
    m_cwdLabel->setText(tr("Terminal unavailable"));
#endif
}

void RepoTerminalPanel::setWorkingDirectory(const QString &repoPath)
{
    m_repoRoot = normalizePath(repoPath);

#if defined(Q_OS_UNIX)
    if (!m_terminal) {
        return;
    }

    if (m_repoRoot.isEmpty()) {
        m_terminal->stopShell();
        m_cwdLabel->setText(tr("No repository open"));
        return;
    }

    m_cwdLabel->setText(tr("Shell in: %1").arg(m_repoRoot));
    if (m_terminal->isRunning()) {
        m_terminal->stopShell();
    }
    if (isVisible()) {
        ensureShellStarted();
    }
#else
    Q_UNUSED(m_repoRoot);
#endif
}

void RepoTerminalPanel::ensureShellStarted()
{
#if defined(Q_OS_UNIX)
    if (!m_terminal || m_repoRoot.isEmpty()) {
        return;
    }

    if (m_terminal->isRunning()) {
        m_terminal->syncDisplaySize();
        return;
    }

    m_cwdLabel->setText(tr("Shell in: %1").arg(m_repoRoot));
    if (!m_terminal->startShell(m_repoRoot)) {
        m_cwdLabel->setText(tr("Failed to start shell in: %1").arg(m_repoRoot));
    }
#endif
}

void RepoTerminalPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        ensureShellStarted();
        focusInput();
    });
}

void RepoTerminalPanel::focusInput()
{
#if defined(Q_OS_UNIX)
    if (m_terminal) {
        m_terminal->setFocus();
    }
#endif
}

void RepoTerminalPanel::onShellExited(int exitCode)
{
    if (exitCode == 0) {
        emit repositoryMayHaveChanged();
    }
}
