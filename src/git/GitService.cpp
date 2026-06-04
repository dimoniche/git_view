#include "git/GitService.h"

#include "git/DiffParser.h"
#include "git/LogParser.h"
#include "git/StatusParser.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>

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

QStringList uniquePaths(QStringList paths)
{
    paths.removeAll(QString());
    paths.removeDuplicates();
    return paths;
}

QStringList pathsForStagedDiff(const GitProcessRunner &runner,
                               const QString &repoPath,
                               const QString &statusPath)
{
    QStringList paths = pathsForDiff(statusPath);
    const QString normalized = StatusParser::normalizeGitPath(statusPath);

    if (!normalized.startsWith(QStringLiteral("./"))) {
        paths.append(QStringLiteral("./") + normalized);
    }

    const GitProcessResult nameResult =
        runner.run(repoPath, {QStringLiteral("diff"), QStringLiteral("--cached"),
                              QStringLiteral("--name-only")});
    if (nameResult.success()) {
        for (const QString &line : nameResult.stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QString name = StatusParser::normalizeGitPath(line);
            if (name.compare(normalized, Qt::CaseInsensitive) == 0) {
                paths.prepend(name);
            }
        }
    }

    return uniquePaths(paths);
}

bool stagedPathDiffers(const GitProcessRunner &runner,
                     const QString &repoPath,
                     const QString &path)
{
    const GitProcessResult result =
        runner.run(repoPath, {QStringLiteral("diff"), QStringLiteral("--cached"),
                              QStringLiteral("--quiet"), QStringLiteral("--"), path});
    return result.exitCode == 1;
}

bool pathIsGitlink(const GitProcessRunner &runner,
                 const QString &repoPath,
                 const QString &path)
{
    const GitProcessResult result =
        runner.run(repoPath, {QStringLiteral("ls-files"), QStringLiteral("-s"), QStringLiteral("--"), path});
    if (!result.success() || result.stdoutText.trimmed().isEmpty()) {
        return false;
    }
    return result.stdoutText.trimmed().startsWith(QStringLiteral("160000"));
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

    const QStringList remoteNames = remotes(repoPath);

    QHash<QString, QString> upstreamByBranch;
    const GitProcessResult upstreamResult = m_runner.run(
        repoPath, {QStringLiteral("for-each-ref"), QStringLiteral("refs/heads/"),
                   QStringLiteral("--format=%(refname:short)|%(upstream:short)")});
    if (upstreamResult.success()) {
        for (QString line :
             upstreamResult.stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            line = line.trimmed();
            const int sep = line.indexOf(QLatin1Char('|'));
            if (sep < 0) {
                continue;
            }
            const QString name = line.left(sep).trimmed();
            const QString upstream = line.mid(sep + 1).trimmed();
            if (!name.isEmpty() && !upstream.isEmpty()) {
                upstreamByBranch.insert(name, upstream);
            }
        }
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
        for (const QString &remote : remoteNames) {
            if (!remote.isEmpty() && branch.name.startsWith(remote + QLatin1Char('/'))) {
                branch.isRemote = true;
                break;
            }
        }
        if (!branch.isRemote) {
            branch.upstream = upstreamByBranch.value(branch.name);
        }
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

bool GitService::hasStagedChanges(const QString &repoPath) const
{
    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("diff"), QStringLiteral("--cached"),
                                QStringLiteral("--quiet")});
    return result.exitCode == 1;
}

GitProcessResult GitService::stageAll(const QString &repoPath) const
{
    m_lastError.clear();
    const GitProcessResult result = m_runner.run(repoPath, {QStringLiteral("add"), QStringLiteral("-A")});
    if (!result.success()) {
        setError(QStringLiteral("git add failed"), result);
    }
    return result;
}

GitProcessResult GitService::discardAllChanges(const QString &repoPath) const
{
    m_lastError.clear();

    const GitProcessResult reset =
        m_runner.run(repoPath, {QStringLiteral("reset"), QStringLiteral("--hard"), QStringLiteral("HEAD")});
    if (!reset.success()) {
        setError(QStringLiteral("git reset --hard failed"), reset);
        return reset;
    }

    const GitProcessResult clean =
        m_runner.run(repoPath, {QStringLiteral("clean"), QStringLiteral("-fd")});
    if (!clean.success()) {
        setError(QStringLiteral("git clean failed"), clean);
    }
    return clean;
}

GitProcessResult GitService::discardFileChanges(const QString &repoPath,
                                              const WorkingTreeChange &change) const
{
    m_lastError.clear();

    const QStringList paths = pathsForDiff(change.path);
    if (paths.isEmpty()) {
        m_lastError = QStringLiteral("File path is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const QString &primaryPath = paths.front();

    if (change.isUntracked()) {
        QStringList args{QStringLiteral("clean"), QStringLiteral("-ff"), QStringLiteral("-d"),
                         QStringLiteral("--")};
        args.append(paths);
        const GitProcessResult result = m_runner.run(repoPath, args);
        if (!result.success()) {
            setError(QStringLiteral("git clean failed"), result);
        }
        return result;
    }

    if (pathIsGitlink(m_runner, repoPath, primaryPath)) {
        const QString subRepoPath = QDir(repoPath).filePath(primaryPath);
        if (isRepository(subRepoPath)) {
            GitProcessResult subReset =
                m_runner.run(subRepoPath, {QStringLiteral("reset"), QStringLiteral("--hard"),
                                           QStringLiteral("HEAD")});
            if (!subReset.success()) {
                setError(QStringLiteral("git reset in nested repository failed"), subReset);
                return subReset;
            }
            m_runner.run(subRepoPath, {QStringLiteral("clean"), QStringLiteral("-fd")});
        }

        QStringList restoreArgs{QStringLiteral("restore"), QStringLiteral("--source=HEAD"),
                                QStringLiteral("--staged"), QStringLiteral("--worktree"),
                                QStringLiteral("--")};
        restoreArgs.append(paths);
        const GitProcessResult result = m_runner.run(repoPath, restoreArgs);
        if (!result.success()) {
            setError(QStringLiteral("git restore failed"), result);
        }
        return result;
    }

    QStringList args{QStringLiteral("restore"), QStringLiteral("--source=HEAD"),
                     QStringLiteral("--staged"), QStringLiteral("--worktree"), QStringLiteral("--")};
    args.append(paths);
    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git restore failed"), result);
    }
    return result;
}

bool GitService::branchExists(const QString &repoPath, const QString &name) const
{
    if (name.isEmpty()) {
        return false;
    }

    GitProcessResult local =
        m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                                QStringLiteral("--quiet"), QStringLiteral("refs/heads/") + name});
    if (local.success()) {
        return true;
    }

    const GitProcessResult any =
        m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                                QStringLiteral("--quiet"), name});
    return any.success();
}

QString GitService::validateBranchName(const QString &repoPath, const QString &name) const
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Branch name is empty");
    }
    if (trimmed.contains(QLatin1Char(' '))) {
        return QStringLiteral("Branch name cannot contain spaces");
    }
    if (trimmed.contains(QStringLiteral(".."))) {
        return QStringLiteral("Branch name cannot contain '..'");
    }
    if (trimmed.startsWith(QLatin1Char('/')) || trimmed.endsWith(QLatin1Char('/'))
        || trimmed.contains(QStringLiteral("//"))) {
        return QStringLiteral("Invalid branch name");
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("check-ref-format"), QStringLiteral("--branch"), trimmed});
    if (!result.success()) {
        const QString detail = result.stderrText.trimmed();
        return detail.isEmpty() ? QStringLiteral("Invalid branch name") : detail;
    }
    return {};
}

GitProcessResult GitService::createBranch(const QString &repoPath,
                                          const QString &name,
                                          const QString &startPoint) const
{
    m_lastError.clear();

    const QString validation = validateBranchName(repoPath, name);
    if (!validation.isEmpty()) {
        m_lastError = validation;
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    if (branchExists(repoPath, name.trimmed())) {
        m_lastError = QStringLiteral("Branch already exists: %1").arg(name.trimmed());
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    QStringList args{QStringLiteral("branch"), name.trimmed()};
    if (!startPoint.trimmed().isEmpty()) {
        args << startPoint.trimmed();
    }

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git branch failed"), result);
    }
    return result;
}

GitProcessResult GitService::checkoutBranch(const QString &repoPath, const QString &name) const
{
    m_lastError.clear();

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("checkout"), name.trimmed()});
    if (!result.success()) {
        setError(QStringLiteral("git checkout failed"), result);
    }
    return result;
}

GitProcessResult GitService::createBranchAndCheckout(const QString &repoPath,
                                                       const QString &name,
                                                       const QString &startPoint) const
{
    m_lastError.clear();

    const QString validation = validateBranchName(repoPath, name);
    if (!validation.isEmpty()) {
        m_lastError = validation;
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    if (branchExists(repoPath, name.trimmed())) {
        m_lastError = QStringLiteral("Branch already exists: %1").arg(name.trimmed());
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    QStringList args{QStringLiteral("checkout"), QStringLiteral("-b"), name.trimmed()};
    if (!startPoint.trimmed().isEmpty()) {
        args << startPoint.trimmed();
    }

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git checkout -b failed"), result);
    }
    return result;
}

GitProcessResult GitService::deleteBranch(const QString &repoPath,
                                          const QString &branchName,
                                          bool isRemote,
                                          bool force) const
{
    m_lastError.clear();

    const QString trimmed = branchName.trimmed();
    if (trimmed.isEmpty()) {
        m_lastError = QStringLiteral("Branch name is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    if (trimmed == currentBranch(repoPath)) {
        m_lastError = QStringLiteral("Cannot delete the current branch");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    if (isRemote) {
        const int slash = trimmed.indexOf(QLatin1Char('/'));
        if (slash <= 0) {
            m_lastError = QStringLiteral("Invalid remote branch name: %1").arg(trimmed);
            GitProcessResult result;
            result.exitCode = 1;
            return result;
        }

        const QString remote = trimmed.left(slash);
        const QString name = trimmed.mid(slash + 1);
        const GitProcessResult result =
            m_runner.run(repoPath, {QStringLiteral("push"), remote, QStringLiteral("--delete"), name});
        if (!result.success()) {
            setError(QStringLiteral("git push --delete failed"), result);
        }
        return result;
    }

    QStringList args{QStringLiteral("branch")};
    args << (force ? QStringLiteral("-D") : QStringLiteral("-d")) << trimmed;
    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git branch delete failed"), result);
    }
    return result;
}

QStringList GitService::remotes(const QString &repoPath) const
{
    const GitProcessResult result = m_runner.run(repoPath, {QStringLiteral("remote")});
    if (!result.success()) {
        return {};
    }

    QStringList names =
        result.stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString &name : names) {
        name = name.trimmed();
    }
    return names;
}

QString GitService::defaultRemote(const QString &repoPath) const
{
    const QStringList names = remotes(repoPath);
    if (names.contains(QStringLiteral("origin"))) {
        return QStringLiteral("origin");
    }
    if (!names.isEmpty()) {
        return names.front();
    }
    return {};
}

std::vector<GitRemote> GitService::listRemotes(const QString &repoPath) const
{
    std::vector<GitRemote> list;
    for (const QString &name : remotes(repoPath)) {
        GitRemote remote;
        remote.name = name;

        const GitProcessResult fetchUrl =
            m_runner.run(repoPath, {QStringLiteral("remote"), QStringLiteral("get-url"), name});
        if (fetchUrl.success()) {
            remote.fetchUrl = fetchUrl.stdoutText.trimmed();
        }

        const GitProcessResult pushUrl = m_runner.run(
            repoPath, {QStringLiteral("remote"), QStringLiteral("get-url"), QStringLiteral("--push"),
                       name});
        if (pushUrl.success()) {
            const QString push = pushUrl.stdoutText.trimmed();
            if (!push.isEmpty() && push != remote.fetchUrl) {
                remote.pushUrl = push;
            }
        }

        list.push_back(std::move(remote));
    }
    return list;
}

QString GitService::validateRemoteName(const QString &name) const
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Remote name is empty");
    }
    if (trimmed.contains(QLatin1Char(' '))) {
        return QStringLiteral("Remote name cannot contain spaces");
    }
    if (trimmed.contains(QLatin1String(".."))) {
        return QStringLiteral("Remote name cannot contain \"..\"");
    }
    return {};
}

GitProcessResult GitService::addRemote(const QString &repoPath,
                                       const QString &name,
                                       const QString &url) const
{
    m_lastError.clear();

    const QString validation = validateRemoteName(name);
    if (!validation.isEmpty()) {
        m_lastError = validation;
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const QString trimmedUrl = url.trimmed();
    if (trimmedUrl.isEmpty()) {
        m_lastError = QStringLiteral("Remote URL is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const QString trimmedName = name.trimmed();
    if (remotes(repoPath).contains(trimmedName)) {
        m_lastError = QStringLiteral("Remote \"%1\" already exists").arg(trimmedName);
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const GitProcessResult result = m_runner.run(
        repoPath, {QStringLiteral("remote"), QStringLiteral("add"), trimmedName, trimmedUrl});
    if (!result.success()) {
        setError(QStringLiteral("git remote add failed"), result);
    }
    return result;
}

GitProcessResult GitService::removeRemote(const QString &repoPath, const QString &name) const
{
    m_lastError.clear();

    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        m_lastError = QStringLiteral("Remote name is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("remote"), QStringLiteral("remove"), trimmedName});
    if (!result.success()) {
        setError(QStringLiteral("git remote remove failed"), result);
    }
    return result;
}

GitProcessResult GitService::setRemoteUrl(const QString &repoPath,
                                          const QString &name,
                                          const QString &url,
                                          bool push) const
{
    m_lastError.clear();

    const QString trimmedName = name.trimmed();
    const QString trimmedUrl = url.trimmed();
    if (trimmedName.isEmpty() || trimmedUrl.isEmpty()) {
        m_lastError = QStringLiteral("Remote name or URL is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    QStringList args{QStringLiteral("remote"), QStringLiteral("set-url")};
    if (push) {
        args << QStringLiteral("--push");
    }
    args << trimmedName << trimmedUrl;

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git remote set-url failed"), result);
    }
    return result;
}

GitProcessResult GitService::publishBranch(const QString &repoPath,
                                           const QString &branchName,
                                           const QString &remote) const
{
    m_lastError.clear();

    const QString trimmedBranch = branchName.trimmed();
    const QString trimmedRemote = remote.trimmed();
    if (trimmedBranch.isEmpty() || trimmedRemote.isEmpty()) {
        m_lastError = QStringLiteral("Branch or remote name is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const GitProcessResult result = m_runner.run(
        repoPath, {QStringLiteral("push"), QStringLiteral("-u"), trimmedRemote, trimmedBranch});
    if (!result.success()) {
        setError(QStringLiteral("git push -u failed"), result);
    }
    return result;
}

GitProcessResult GitService::pushBranch(const QString &repoPath,
                                        const QString &branchName,
                                        const QString &remote) const
{
    m_lastError.clear();

    const QString trimmedBranch = branchName.trimmed();
    const QString trimmedRemote = remote.trimmed();
    if (trimmedBranch.isEmpty() || trimmedRemote.isEmpty()) {
        m_lastError = QStringLiteral("Branch or remote name is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("push"), trimmedRemote, trimmedBranch});
    if (!result.success()) {
        setError(QStringLiteral("git push failed"), result);
    }
    return result;
}

GitProcessResult GitService::commit(const QString &repoPath, const QString &message) const
{
    m_lastError.clear();

    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty()) {
        m_lastError = QStringLiteral("Commit message is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("commit"), QStringLiteral("-m"), trimmed});
    if (!result.success()) {
        setError(QStringLiteral("git commit failed"), result);
    }
    return result;
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
    m_lastDiffCommand =
        QStringLiteral("git -C ") + QDir::toNativeSeparators(repoPath) + QLatin1Char(' ')
        + args.join(QLatin1Char(' '));

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.stdoutText.trimmed().isEmpty()) {
        return result.stdoutText;
    }
    if (!result.diffSucceeded()) {
        setError(QStringLiteral("git diff failed"), result);
    }
    return {};
}

QString GitService::stagedDiffForPath(const QString &repoPath, const QString &path) const
{
    const QString normalizedPath = StatusParser::normalizeGitPath(path);
    const QStringList diffPaths = pathsForStagedDiff(m_runner, repoPath, normalizedPath);

    QStringList pathsWithStagedDiff;
    for (const QString &diffPath : diffPaths) {
        if (stagedPathDiffers(m_runner, repoPath, diffPath)) {
            pathsWithStagedDiff.append(diffPath);
        }
    }
    if (pathsWithStagedDiff.isEmpty()) {
        return {};
    }

    const bool headExists =
        m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                                QStringLiteral("HEAD")})
            .success();

    for (const QString &diffPath : pathsWithStagedDiff) {
        const GitProcessResult quick =
            m_runner.run(repoPath, {QStringLiteral("diff"), QStringLiteral("-U3"),
                                    QStringLiteral("--cached"), QStringLiteral("--"), diffPath});
        m_lastDiffCommand = QStringLiteral("git -C ") + QDir::toNativeSeparators(repoPath)
                            + QStringLiteral(" diff -U3 --cached -- ") + diffPath;
        if (!quick.stdoutText.trimmed().isEmpty()) {
            return quick.stdoutText;
        }

        if (headExists) {
            const GitProcessResult colon =
                m_runner.run(repoPath, {QStringLiteral("diff"), QStringLiteral("-U3"),
                                        QStringLiteral("HEAD:") + diffPath,
                                        QStringLiteral(":0:") + diffPath});
            m_lastDiffCommand = QStringLiteral("git -C ") + QDir::toNativeSeparators(repoPath)
                                + QStringLiteral(" diff -U3 HEAD:") + diffPath + QStringLiteral(" :0:")
                                + diffPath;
            if (!colon.stdoutText.trimmed().isEmpty()) {
                return colon.stdoutText;
            }
        }
    }

    auto tryPathDiff = [&](const QStringList &extraArgs) -> QString {
        for (const QString &diffPath : pathsWithStagedDiff) {
            QStringList args{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                             QStringLiteral("--text"), QStringLiteral("--unified=3")};
            args << extraArgs << QStringLiteral("--") << diffPath;
            QString patch = runDiffCommand(repoPath, args);
            if (!patch.trimmed().isEmpty()) {
                return patch;
            }
            m_lastError.clear();

            QStringList plainArgs{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                                  QStringLiteral("--unified=3")};
            plainArgs << extraArgs << QStringLiteral("--") << diffPath;
            patch = runDiffCommand(repoPath, plainArgs);
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

    if (headExists) {
        auto tryColonDiff = [&](const QString &left, const QString &right) -> QString {
            return runDiffCommand(repoPath, {QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                                             QStringLiteral("--unified=3"), left, right});
        };

        if (pathsWithStagedDiff.size() >= 2) {
            patch = tryColonDiff(QStringLiteral("HEAD:") + pathsWithStagedDiff.front(),
                                 QStringLiteral(":0:") + pathsWithStagedDiff.back());
            if (!patch.isEmpty()) {
                return patch;
            }
            m_lastError.clear();
        }

        for (const QString &diffPath : pathsWithStagedDiff) {
            patch = tryColonDiff(QStringLiteral("HEAD:") + diffPath,
                                 QStringLiteral(":0:") + diffPath);
            if (!patch.isEmpty()) {
                return patch;
            }
            m_lastError.clear();
        }
    }

    QStringList fullArgs{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                         QStringLiteral("--text"), QStringLiteral("--unified=3"),
                         QStringLiteral("--cached")};
    patch = runDiffCommand(repoPath, fullArgs);
    patch = DiffParser::extractFilePatch(patch, normalizedPath);
    if (!patch.isEmpty()) {
        return patch;
    }
    m_lastError.clear();

    if (headExists) {
        QStringList indexArgs{QStringLiteral("diff-index"), QStringLiteral("-p"),
                              QStringLiteral("--cached"), QStringLiteral("HEAD"),
                              QStringLiteral("--")};
        indexArgs.append(pathsWithStagedDiff);
        patch = runDiffCommand(repoPath, indexArgs);
        if (!patch.isEmpty()) {
            return patch;
        }
        m_lastError.clear();
    }

    for (const QString &diffPath : pathsWithStagedDiff) {
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
