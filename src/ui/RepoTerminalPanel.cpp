#include "ui/RepoTerminalPanel.h"

#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QTextCursor>
#include <QVBoxLayout>

namespace {

QString shellProgram()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("cmd.exe");
#else
    return QStringLiteral("/bin/sh");
#endif
}

QStringList shellArguments(const QString &command)
{
#if defined(Q_OS_WIN)
    return {QStringLiteral("/c"), command};
#else
    return {QStringLiteral("-c"), command};
#endif
}

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

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    QFont font = m_output->font();
    font.setStyleHint(QFont::Monospace);
    font.setFamily(QStringLiteral("Menlo"));
#if defined(Q_OS_WIN)
    font.setFamily(QStringLiteral("Consolas"));
#elif defined(Q_OS_LINUX)
    font.setFamily(QStringLiteral("Monospace"));
#endif
    m_output->setFont(font);
    m_output->setMaximumBlockCount(5000);
    layout->addWidget(m_output, 1);

    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(4);
    inputRow->addWidget(new QLabel(QStringLiteral("$"), this));

    m_input = new QLineEdit(this);
    m_input->setFont(font);
    m_input->setPlaceholderText(tr("git status, git pull, shell commands…"));
    connect(m_input, &QLineEdit::returnPressed, this, &RepoTerminalPanel::runCurrentCommand);
    m_input->installEventFilter(this);
    inputRow->addWidget(m_input, 1);
    layout->addLayout(inputRow);

    appendOutput(tr("Repository terminal — commands run in the repository folder."));
    setInputEnabled(false);
}

void RepoTerminalPanel::setWorkingDirectory(const QString &repoPath)
{
    const QString root = normalizePath(repoPath);
    m_repoRoot = root;
    m_cwd = root;

    if (root.isEmpty()) {
        m_cwdLabel->setText(tr("No repository open"));
        appendOutput(tr("Open a repository to run commands."));
        setInputEnabled(false);
        return;
    }

    m_cwdLabel->setText(tr("Working directory: %1").arg(m_cwd));
    appendOutput(tr("--- Repository: %1 ---").arg(m_repoRoot));
    setInputEnabled(true);
    focusInput();
}

void RepoTerminalPanel::focusInput()
{
    if (m_input) {
        m_input->setFocus();
    }
}

void RepoTerminalPanel::appendOutput(const QString &text, bool isError)
{
    if (!m_output || text.isEmpty()) {
        return;
    }

    QTextCharFormat format;
    if (isError) {
        format.setForeground(Qt::red);
    }

    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (isError) {
        cursor.mergeCharFormat(format);
    }
    cursor.insertText(text);
    if (!text.endsWith(QLatin1Char('\n'))) {
        cursor.insertText(QStringLiteral("\n"));
    }
    m_output->setTextCursor(cursor);
    m_output->ensureCursorVisible();
}

void RepoTerminalPanel::setInputEnabled(bool enabled)
{
    if (m_input) {
        m_input->setEnabled(enabled);
    }
}

bool RepoTerminalPanel::applyChangeDirectory(const QString &command)
{
    const QString trimmed = command.trimmed();
    static const QRegularExpression cdRegex(
        QStringLiteral("^cd\\s+(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = cdRegex.match(trimmed);
    if (!match.hasMatch()) {
        return false;
    }

    QString target = match.captured(1).trimmed();
    if (target.startsWith(QLatin1Char('"')) && target.endsWith(QLatin1Char('"')) && target.size() >= 2) {
        target = target.mid(1, target.size() - 2);
    }

    QString newDir;
    if (target == QLatin1String("..")) {
        newDir = normalizePath(QDir(m_cwd).filePath(QStringLiteral("..")));
    } else if (target == QLatin1Char('~')) {
        newDir = normalizePath(QDir::homePath());
    } else if (target.startsWith(QLatin1Char('/')) || target.contains(QLatin1Char(':'))) {
        newDir = normalizePath(target);
    } else if (target == QLatin1String(".") || target.isEmpty()) {
        newDir = m_cwd;
    } else {
        newDir = normalizePath(QDir(m_cwd).filePath(target));
    }

    if (!QDir(newDir).exists()) {
        appendOutput(tr("cd: no such directory: %1").arg(target), true);
        return true;
    }

    m_cwd = newDir;
    m_cwdLabel->setText(tr("Working directory: %1").arg(m_cwd));
    appendOutput(tr("Changed directory to %1").arg(m_cwd));
    return true;
}

bool RepoTerminalPanel::commandLikelyChangesRepository(const QString &command) const
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (trimmed.startsWith(QLatin1String("git"), Qt::CaseInsensitive)) {
        static const QRegularExpression readOnlyGit(
            QStringLiteral("^git\\s+(log|show|diff|status|branch\\s*($|[-v])|remote|rev-parse|"
                           "describe|shortlog|grep|blame|ls-files|ls-tree|cat-file|help|"
                           "version|--version|config\\s+--get|for-each-ref)"),
            QRegularExpression::CaseInsensitiveOption);
        if (readOnlyGit.match(trimmed).hasMatch()) {
            return false;
        }
        return true;
    }

    static const QStringList mutatingPrefixes = {
        QStringLiteral("make"),
        QStringLiteral("cmake"),
        QStringLiteral("npm"),
        QStringLiteral("touch"),
        QStringLiteral("rm"),
        QStringLiteral("mv"),
        QStringLiteral("cp"),
    };
    for (const QString &prefix : mutatingPrefixes) {
        if (trimmed.startsWith(prefix, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

void RepoTerminalPanel::runCurrentCommand()
{
    if (!m_input || m_cwd.isEmpty()) {
        return;
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        appendOutput(tr("Wait for the current command to finish."), true);
        return;
    }

    const QString command = m_input->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    if (!m_history.isEmpty() && m_history.last() != command) {
        m_history.append(command);
    } else if (m_history.isEmpty()) {
        m_history.append(command);
    }
    m_historyIndex = -1;
    m_savedInput.clear();
    m_input->clear();

    appendOutput(QStringLiteral("$ ") + command);

    if (applyChangeDirectory(command)) {
        return;
    }

    if (command == QLatin1String("clear") || command == QLatin1String("cls")) {
        m_output->clear();
        return;
    }

    setInputEnabled(false);

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_cwd);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyRead, this, &RepoTerminalPanel::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &RepoTerminalPanel::onProcessFinished);

    m_process->start(shellProgram(), shellArguments(command));
    if (!m_process->waitForStarted(3000)) {
        appendOutput(tr("Failed to start shell: %1").arg(m_process->errorString()), true);
        setInputEnabled(true);
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void RepoTerminalPanel::onProcessOutput()
{
    if (!m_process) {
        return;
    }
    appendOutput(QString::fromLocal8Bit(m_process->readAll()), false);
}

void RepoTerminalPanel::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);

    if (exitCode != 0) {
        appendOutput(tr("Exit code: %1").arg(exitCode), true);
    }

    const QString command = m_history.isEmpty() ? QString() : m_history.last();
    if (exitCode == 0 && commandLikelyChangesRepository(command)) {
        emit repositoryMayHaveChanged();
    }

    setInputEnabled(true);
    focusInput();

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}

bool RepoTerminalPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            if (m_history.isEmpty()) {
                return QWidget::eventFilter(watched, event);
            }
            if (m_historyIndex < 0) {
                m_savedInput = m_input->text();
                m_historyIndex = m_history.size() - 1;
            } else if (m_historyIndex > 0) {
                --m_historyIndex;
            }
            m_input->setText(m_history.at(m_historyIndex));
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down) {
            if (m_historyIndex < 0) {
                return QWidget::eventFilter(watched, event);
            }
            if (m_historyIndex < m_history.size() - 1) {
                ++m_historyIndex;
                m_input->setText(m_history.at(m_historyIndex));
            } else {
                m_historyIndex = -1;
                m_input->setText(m_savedInput);
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
