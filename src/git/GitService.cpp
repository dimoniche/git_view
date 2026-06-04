#include "git/GitService.h"

#include "git/DiffParser.h"
#include "git/LogParser.h"
#include "git/StatusParser.h"

#include <QDir>
#include <QFileInfo>

namespace {

QStringList pathsForDiff(const QString &statusPath)
{
    const QString normalized = StatusParser::normalizeGitPath(statusPath);
    const int arrow = normalized.indexOf(QStringLiteral(" -> "));
    if (arrow >= 0) {
        return {StatusParser::normalizeGitPath(normalized.left(arrow)),
                StatusParser::normalizeGitPath(normalized.mid(arrow + 4))};
    }
    return {normalized};
}

constexpr const char kEmptyTreeHash[] = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";

} // namespace

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
        m_runner.run(repoPath, {QStringLiteral("diff-tree"), QStringLiteral("--root"),
                                QStringLiteral("--no-commit-id"), QStringLiteral("--name-status"),
                                QStringLiteral("-r"), hash});
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

std::vector<WorkingTreeChange> GitService::workingTreeChanges(const QString &repoPath) const
{
    m_lastError.clear();
    std::vector<WorkingTreeChange> changes;

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("status"), QStringLiteral("--porcelain")});
    if (!result.success()) {
        setError(QStringLiteral("git status failed"), result);
        return changes;
    }

    const QStringList lines = result.stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        if (line.size() < 3) {
            continue;
        }

        WorkingTreeChange change;
        if (!StatusParser::parsePorcelainLine(line, &change)) {
            continue;
        }
        changes.push_back(std::move(change));
    }

    return changes;
}

QString GitService::runDiffCommand(const QString &repoPath, const QStringList &args) const
{
    m_lastDiffCommand = QStringLiteral("git ") + args.join(QLatin1Char(' '));

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.diffSucceeded()) {
        setError(QStringLiteral("git diff failed"), result);
        return {};
    }
    return result.stdoutText;
}

QString GitService::stagedDiffForPath(const QString &repoPath, const QString &path) const
{
    const QString normalizedPath = StatusParser::normalizeGitPath(path);
    const QStringList diffPaths = pathsForDiff(normalizedPath);

    auto tryPathDiff = [&](const QStringList &extraArgs) -> QString {
        for (const QString &diffPath : diffPaths) {
            QStringList args{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                             QStringLiteral("--text"), QStringLiteral("--unified=3")};
            args << extraArgs << QStringLiteral("--") << QStringList{diffPath};
            const QString patch = runDiffCommand(repoPath, args);
            if (!patch.trimmed().isEmpty()) {
                return patch;
            }
            m_lastError.clear();
        }
        return {};
    };

    QString patch = tryPathDiff({QStringLiteral("--cached")});
    if (!patch.isEmpty()) {
        return patch;
    }

    patch = tryPathDiff({QStringLiteral("--staged")});
    if (!patch.isEmpty()) {
        return patch;
    }

    QStringList indexArgs{QStringLiteral("diff-index"), QStringLiteral("-p"),
                          QStringLiteral("--cached"), QStringLiteral("HEAD"),
                          QStringLiteral("--")};
    indexArgs.append(diffPaths);
    patch = runDiffCommand(repoPath, indexArgs);
    if (!patch.isEmpty()) {
        return patch;
    }
    m_lastError.clear();

    QStringList fullArgs{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                         QStringLiteral("--text"), QStringLiteral("--unified=3"),
                         QStringLiteral("--cached")};
    patch = runDiffCommand(repoPath, fullArgs);
    patch = DiffParser::extractFilePatch(patch, normalizedPath);
    if (!patch.isEmpty()) {
        return patch;
    }
    m_lastError.clear();

    for (const QString &diffPath : diffPaths) {
        GitProcessResult indexRev =
            m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("-q"),
                                    QStringLiteral(":0:") + diffPath});
        if (!indexRev.success()) {
            indexRev = m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("-q"),
                                               QStringLiteral(":") + diffPath});
        }
        if (!indexRev.success()) {
            continue;
        }

        const QString stagedHash = indexRev.stdoutText.trimmed();
        GitProcessResult headRev =
            m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("-q"),
                                    QStringLiteral("HEAD:") + diffPath});

        const QString headHash =
            headRev.success() ? headRev.stdoutText.trimmed() : QString::fromLatin1(kEmptyTreeHash);

        if (headHash == stagedHash) {
            continue;
        }

        patch = runDiffCommand(
            repoPath, {QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                       QStringLiteral("--text"), QStringLiteral("--unified=3"), headHash, stagedHash});
        if (!patch.isEmpty()) {
            return patch;
        }
        m_lastError.clear();
    }

    return {};
}

QString GitService::workingTreeFileDiff(const QString &repoPath,
                                        const QString &path,
                                        WorkingDiffScope scope,
                                        const WorkingTreeChange &change) const
{
    m_lastError.clear();
    m_lastDiffCommand.clear();

    const QString normalizedPath = StatusParser::normalizeGitPath(path);
    if (normalizedPath.isEmpty()) {
        return {};
    }

    const QStringList diffPaths = pathsForDiff(normalizedPath);

    auto runDiff = [&](const QStringList &extraArgs, const QStringList &paths) -> QString {
        QStringList args{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                         QStringLiteral("--text"), QStringLiteral("--unified=3")};
        args << extraArgs;
        if (!paths.isEmpty()) {
            args << QStringLiteral("--") << paths;
        }
        return runDiffCommand(repoPath, args);
    };

    switch (scope) {
    case WorkingDiffScope::Staged:
        return stagedDiffForPath(repoPath, normalizedPath);
    case WorkingDiffScope::AgainstHead:
        return runDiff({QStringLiteral("HEAD")}, diffPaths);
    case WorkingDiffScope::Unstaged:
        break;
    }

    if (change.isUntracked()) {
        const QString absPath = QDir(repoPath).filePath(diffPaths.front());
        m_lastDiffCommand.clear();
        QStringList args;
        args << QStringLiteral("diff") << QStringLiteral("--unified=3") << QStringLiteral("--no-index")
             << QStringLiteral("/dev/null") << absPath;

        const GitProcessResult result = m_runner.run(repoPath, args);
        if (!result.diffSucceeded()) {
            setError(QStringLiteral("git diff failed"), result);
            return {};
        }
        return result.stdoutText;
    }

    QString diff = runDiff({}, diffPaths);
    if (!diff.isEmpty() || !m_lastError.isEmpty()) {
        return diff;
    }

    if (change.hasStaged()) {
        m_lastError.clear();
        diff = runDiff({QStringLiteral("--cached")}, diffPaths);
    }
    return diff;
}

QString GitService::commitFileDiff(const QString &repoPath,
                                   const QString &hash,
                                   const QString &path) const
{
    m_lastError.clear();

    if (path.isEmpty()) {
        return {};
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("show"), QStringLiteral("--format="),
                                QStringLiteral("--unified=3"), hash, QStringLiteral("--"), path});
    if (!result.success()) {
        setError(QStringLiteral("git show failed"), result);
        return {};
    }

    QString diff = result.stdoutText;
    if (diff.isEmpty() && result.stderrText.contains(QStringLiteral("Binary files"))) {
        diff = result.stderrText.trimmed();
    }
    return diff;
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
