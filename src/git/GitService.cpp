#include "git/GitService.h"

#include "git/LogParser.h"

#include <QDir>
#include <QFileInfo>

GitService::GitService(GitProcessRunner runner)
    : m_runner(std::move(runner))
{
}

bool GitService::isRepository(const QString &path) const
{
    const GitProcessResult result =
        m_runner.run(path, {QStringLiteral("rev-parse"), QStringLiteral("--git-dir")});
    return result.success() && !result.stdoutText.trimmed().isEmpty();
}

QString GitService::discoverGitDir(const QString &path) const
{
    const GitProcessResult result =
        m_runner.run(path, {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")});
    if (!result.success()) {
        setError(QStringLiteral("Not a git repository"), result);
        return {};
    }
    return result.stdoutText.trimmed();
}

std::vector<Commit> GitService::logAll(const QString &repoPath, int maxCount) const
{
    return runLog(repoPath, {QStringLiteral("--all")}, maxCount);
}

std::vector<Commit> GitService::logBranch(const QString &repoPath,
                                          const QString &branch,
                                          int maxCount) const
{
    return runLog(repoPath, {branch}, maxCount);
}

std::vector<Commit> GitService::runLog(const QString &repoPath,
                                       const QStringList &extraArgs,
                                       int maxCount) const
{
    m_lastError.clear();

    QStringList args;
    args << QStringLiteral("log") << extraArgs
         << QStringLiteral("--date-order")
         << QStringLiteral("-n")
         << QString::number(maxCount)
         << QStringLiteral("--format=%H%x09%P%x09%an%x09%ad%x09%s")
         << QStringLiteral("--date=iso-strict");

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git log failed"), result);
        return {};
    }

    return LogParser::parseLogOutput(result.stdoutText);
}

std::vector<Branch> GitService::branches(const QString &repoPath) const
{
    m_lastError.clear();
    std::vector<Branch> list;

    const QString current = currentBranch(repoPath);

    QStringList args;
    args << QStringLiteral("branch")
         << QStringLiteral("-a")
         << QStringLiteral("--format=%(refname:short)|%(objectname)");

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git branch failed"), result);
        return list;
    }

    const QStringList lines = result.stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        const int sep = line.indexOf(QLatin1Char('|'));
        if (sep < 0) {
            continue;
        }

        Branch branch;
        branch.name = line.left(sep).trimmed();
        branch.tipHash = line.mid(sep + 1).trimmed();
        branch.isRemote = branch.name.startsWith(QLatin1String("origin/"))
                          || branch.name.startsWith(QLatin1String("upstream/"));
        branch.isCurrent = (branch.name == current);
        list.push_back(std::move(branch));
    }

    return list;
}

QString GitService::currentBranch(const QString &repoPath) const
{
    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("branch"), QStringLiteral("--show-current")});
    if (result.success()) {
        return result.stdoutText.trimmed();
    }

    const GitProcessResult head =
        m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
                                QStringLiteral("HEAD")});
    if (head.success()) {
        return head.stdoutText.trimmed();
    }
    return {};
}

bool GitService::hasUncommittedChanges(const QString &repoPath) const
{
    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("status"), QStringLiteral("--porcelain")});
    if (!result.success()) {
        return false;
    }
    return !result.stdoutText.trimmed().isEmpty();
}

CommitDetails GitService::commitDetails(const QString &repoPath, const QString &hash) const
{
    m_lastError.clear();
    CommitDetails details;

    QStringList showArgs;
    showArgs << QStringLiteral("show")
             << QStringLiteral("-s")
             << QStringLiteral("--format=%H%x09%P%x09%an%x09%ad%x09%s")
             << QStringLiteral("--date=iso-strict")
             << hash;

    const GitProcessResult showResult = m_runner.run(repoPath, showArgs);
    if (!showResult.success()) {
        setError(QStringLiteral("git show failed"), showResult);
        return details;
    }

    const std::vector<Commit> parsed = LogParser::parseLogOutput(showResult.stdoutText);
    if (!parsed.empty()) {
        details.commit = parsed.front();
    }

    const GitProcessResult diffResult =
        m_runner.run(repoPath, {QStringLiteral("diff-tree"), QStringLiteral("--no-commit-id"),
                                QStringLiteral("--name-status"), QStringLiteral("-r"), hash});
    if (!diffResult.success()) {
        setError(QStringLiteral("git diff-tree failed"), diffResult);
        return details;
    }

    const QStringList lines = diffResult.stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        const int tab = line.indexOf(QLatin1Char('\t'));
        if (tab < 1) {
            continue;
        }
        CommitFileChange change;
        change.status = line.left(tab).trimmed();
        change.path = line.mid(tab + 1).trimmed();
        details.files.push_back(std::move(change));
    }

    return details;
}

GitProcessResult GitService::mergeAbort(const QString &repoPath) const
{
    m_lastError.clear();
    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("merge"), QStringLiteral("--abort")});
    if (!result.success()) {
        setError(QStringLiteral("git merge --abort failed"), result);
    }
    return result;
}

QStringList GitService::unmergedFiles(const QString &repoPath) const
{
    const GitProcessResult result = m_runner.run(
        repoPath, {QStringLiteral("diff"), QStringLiteral("--name-only"),
                   QStringLiteral("--diff-filter=U")});
    if (!result.success()) {
        return {};
    }
    QStringList files = result.stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString &file : files) {
        file = file.trimmed();
    }
    return files;
}

GitProcessResult GitService::merge(const QString &repoPath,
                                   const QString &branch,
                                   bool noFf) const
{
    m_lastError.clear();

    QStringList args{QStringLiteral("merge"), branch};
    if (noFf) {
        args << QStringLiteral("--no-ff");
    }

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git merge failed"), result);
    }
    return result;
}

void GitService::setError(const QString &message, const GitProcessResult &result) const
{
    m_lastError = message;
    const QString detail = result.stderrText.trimmed();
    if (!detail.isEmpty()) {
        m_lastError += QStringLiteral(": ") + detail;
    }
}
