#include "git/GitService.h"

#include "git/DiffParser.h"
#include "git/LogParser.h"
#include "git/PathUtils.h"
#include "git/StatusParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>

namespace {

QString gitPathInRepo(const QString &repoPath, const QString &path)
{
    QString relative = PathUtils::toRepoRelativePath(repoPath, path);
    if (relative.isEmpty()) {
        relative = PathUtils::normalizeExternalPath(path);
    }
    return StatusParser::normalizeGitPath(relative);
}

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

bool isDetachedHeadLabel(const QString &name)
{
    return name.startsWith(QStringLiteral("(HEAD detached"));
}

struct RemoteEndpoint {
    QString protocol;
    QString host;
    bool valid = false;
};

RemoteEndpoint parseRemoteUrl(const QString &url)
{
    RemoteEndpoint endpoint;
    const QUrl parsed(url);
    if (parsed.isValid()
        && (parsed.scheme() == QStringLiteral("https")
            || parsed.scheme() == QStringLiteral("http"))) {
        endpoint.protocol = parsed.scheme();
        endpoint.host = parsed.host();
        endpoint.valid = !endpoint.host.isEmpty();
        return endpoint;
    }

    if (parsed.isValid() && parsed.scheme() == QStringLiteral("ssh")) {
        endpoint.protocol = QStringLiteral("ssh");
        endpoint.host = parsed.host();
        endpoint.valid = !endpoint.host.isEmpty();
        return endpoint;
    }

    if (url.startsWith(QStringLiteral("git@"))) {
        const int at = url.indexOf(QLatin1Char('@'));
        const int colon = url.indexOf(QLatin1Char(':'));
        if (at >= 0 && colon > at) {
            endpoint.protocol = QStringLiteral("ssh");
            endpoint.host = url.mid(at + 1, colon - at - 1);
            endpoint.valid = !endpoint.host.isEmpty();
        }
    }

    return endpoint;
}

QString credentialRequestForUrl(const QString &url)
{
    const RemoteEndpoint endpoint = parseRemoteUrl(url);
    if (!endpoint.valid) {
        return {};
    }

    return QStringLiteral("protocol=%1\nhost=%2\n\n").arg(endpoint.protocol, endpoint.host);
}

QString credentialApproveForUrl(const QString &url,
                                const QString &username,
                                const QString &password)
{
    const RemoteEndpoint endpoint = parseRemoteUrl(url);
    if (!endpoint.valid) {
        return {};
    }

    return QStringLiteral("protocol=%1\nhost=%2\nusername=%3\npassword=%4\n\n")
        .arg(endpoint.protocol, endpoint.host, username, password);
}

QString credentialPasswordFromOutput(const QString &output)
{
    for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (line.startsWith(QStringLiteral("password="))) {
            return line.mid(QStringLiteral("password=").size());
        }
    }
    return {};
}

} // namespace

bool GitService::isBinaryDiffOutput(const QString &text)
{
    if (text.isEmpty()) {
        return false;
    }

    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("Binary files "))
            && trimmed.endsWith(QStringLiteral(" differ"))) {
            return true;
        }
        if (trimmed.startsWith(QStringLiteral("GIT binary patch"))) {
            return true;
        }
    }
    return false;
}

bool GitService::isWorktreePathBinary(const QString &absolutePath)
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    return file.read(8192).contains('\0');
}

QString GitService::acceptDiffOutput(const QString &diff) const
{
    if (isBinaryDiffOutput(diff)) {
        m_lastDiffIsBinary = true;
        return {};
    }
    return diff;
}

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
    QString gitCwd = PathUtils::normalizeExternalPath(path);
    if (gitCwd.isEmpty()) {
        return {};
    }

    const QFileInfo info(gitCwd);
    if (info.exists() && info.isFile()) {
        gitCwd = info.absolutePath();
    }

    const GitProcessResult result =
        m_runner.run(gitCwd, {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")});
    if (!result.success()) {
        setError(QStringLiteral("Not a git repository"), result);
        return {};
    }
    return result.stdoutText.trimmed();
}

GitProcessResult GitService::initRepository(const QString &path,
                                            const QString &initialBranch) const
{
    m_lastError.clear();

    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        m_lastError = QStringLiteral("Repository path is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    QStringList args{QStringLiteral("init")};
    const QString branch = initialBranch.trimmed();
    if (!branch.isEmpty()) {
        args << QStringLiteral("-b") << branch;
    }

    const GitProcessResult result = m_runner.run(trimmedPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git init failed"), result);
    }
    return result;
}

std::vector<Commit> GitService::logAll(const QString &repoPath, int maxCount) const
{
    return runLog(repoPath, {QStringLiteral("--all")}, maxCount);
}

std::vector<Commit> GitService::logBranch(const QString &repoPath,
                                          const QString &branch,
                                          int maxCount) const
{
    if (isDetachedHeadLabel(branch)) {
        return runLog(repoPath, {QStringLiteral("HEAD")}, maxCount);
    }
    return runLog(repoPath, {branch}, maxCount);
}

std::vector<Commit> GitService::runLog(const QString &repoPath,
                                       const QStringList &extraArgs,
                                       int maxCount) const
{
    m_lastError.clear();

    QStringList args;
    args << QStringLiteral("log") << extraArgs
         << QStringLiteral("--topo-order")
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

int GitService::commitCount(const QString &repoPath, const QString &branch) const
{
    QStringList args{QStringLiteral("rev-list"), QStringLiteral("--count")};
    const QString trimmed = branch.trimmed();
    if (trimmed.isEmpty() || isDetachedHeadLabel(trimmed)) {
        args << QStringLiteral("--all");
    } else {
        args << trimmed;
    }

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        return 0;
    }

    bool ok = false;
    const int count = result.stdoutText.trimmed().toInt(&ok);
    return ok ? count : 0;
}

std::vector<Branch> GitService::branches(const QString &repoPath) const
{
    m_lastError.clear();
    std::vector<Branch> list;

    const QString current = currentBranch(repoPath);
    const bool detached = (current == QLatin1String("HEAD"));
    QString headHash;
    if (detached) {
        const GitProcessResult headResult =
            m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
        if (headResult.success()) {
            headHash = headResult.stdoutText.trimmed();
        }
    }

    QStringList args;
    args << QStringLiteral("for-each-ref")
         << QStringLiteral("refs/heads")
         << QStringLiteral("refs/remotes")
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
        if (isDetachedHeadLabel(branch.name)) {
            continue;
        }
        if (remoteNames.contains(branch.name) || branch.name.endsWith(QStringLiteral("/HEAD"))) {
            continue;
        }
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
        branch.isCurrent =
            !detached && (branch.name == current);
        if (detached && !headHash.isEmpty() && branch.tipHash == headHash && !branch.isRemote) {
            branch.isCurrent = true;
        }
        list.push_back(std::move(branch));
    }

    return list;
}

QString GitService::currentBranch(const QString &repoPath) const
{
    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("branch"), QStringLiteral("--show-current")});
    if (result.success()) {
        const QString branch = result.stdoutText.trimmed();
        if (!branch.isEmpty()) {
            return branch;
        }
    }

    const GitProcessResult head =
        m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
                                QStringLiteral("HEAD")});
    if (head.success()) {
        return head.stdoutText.trimmed();
    }
    return {};
}

bool GitService::isPseudoDetachedBranchName(const QString &name)
{
    return isDetachedHeadLabel(name);
}

QString GitService::displayBranchName(const QString &repoPath) const
{
    const QString branch = currentBranch(repoPath);
    if (branch != QLatin1String("HEAD")) {
        return branch;
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("name-rev"), QStringLiteral("--name-only"),
                                  QStringLiteral("HEAD")});
    if (!result.success()) {
        return QStringLiteral("HEAD (detached)");
    }

    QString name = result.stdoutText.trimmed();
    if (name.startsWith(QStringLiteral("remotes/"))) {
        name = name.mid(QStringLiteral("remotes/").size());
    } else if (name.startsWith(QStringLiteral("tags/"))) {
        name = name.mid(QStringLiteral("tags/").size());
    }
    return QStringLiteral("%1 (detached)").arg(name);
}

BranchSyncCounts GitService::currentBranchSyncCounts(const QString &repoPath) const
{
    BranchSyncCounts counts;

    const QString branch = currentBranch(repoPath);
    if (branch.isEmpty() || branch == QLatin1String("HEAD")) {
        return counts;
    }

    auto parseCountLine = [](const QString &text, BranchSyncCounts *out) -> bool {
        const QStringList parts =
            text.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            return false;
        }
        bool behindOk = false;
        bool aheadOk = false;
        const int behind = parts.at(0).toInt(&behindOk);
        const int ahead = parts.at(1).toInt(&aheadOk);
        if (!behindOk || !aheadOk) {
            return false;
        }
        out->behind = behind;
        out->ahead = ahead;
        out->valid = true;
        return true;
    };

    const QStringList revListBase{QStringLiteral("rev-list"), QStringLiteral("--left-right"),
                                  QStringLiteral("--count")};

    const GitProcessResult upstreamRef = m_runner.run(
        repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"), QStringLiteral("@{u}")});

    QString compareSpec = QStringLiteral("@{u}...HEAD");
    if (upstreamRef.success()) {
        counts.upstream = upstreamRef.stdoutText.trimmed();
    }

    GitProcessResult range =
        m_runner.run(repoPath, revListBase + QStringList{compareSpec});
    if (range.success() && parseCountLine(range.stdoutText, &counts)) {
        return counts;
    }

    counts = BranchSyncCounts{};
    const QString remote = defaultRemote(repoPath);
    if (remote.isEmpty()) {
        return counts;
    }

    const QString remoteBranch = remote + QLatin1Char('/') + branch;
    const GitProcessResult verify = m_runner.run(
        repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                   QStringLiteral("refs/remotes/") + remoteBranch});
    if (!verify.success()) {
        return counts;
    }

    compareSpec = remoteBranch + QStringLiteral("...HEAD");
    range = m_runner.run(repoPath, revListBase + QStringList{compareSpec});
    if (!range.success() || !parseCountLine(range.stdoutText, &counts)) {
        counts = BranchSyncCounts{};
        return counts;
    }

    counts.upstream = remoteBranch;
    return counts;
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

GitProcessResult GitService::addToGitignore(const QString &repoPath, const QString &path) const
{
    m_lastError.clear();

    const QString normalized = StatusParser::normalizeGitPath(path);
    if (normalized.isEmpty()) {
        m_lastError = QStringLiteral("File path is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    QString pattern = normalized;
    const QFileInfo pathInfo(QDir(repoPath).filePath(pattern));
    if (pathInfo.exists() && pathInfo.isDir() && !pattern.endsWith(QLatin1Char('/'))) {
        pattern += QLatin1Char('/');
    }

    const QString gitignorePath = QDir(repoPath).filePath(QStringLiteral(".gitignore"));
    QString content;
    if (QFile::exists(gitignorePath)) {
        QFile readFile(gitignorePath);
        if (!readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_lastError = QStringLiteral("Could not read .gitignore");
            GitProcessResult result;
            result.exitCode = 1;
            return result;
        }
        content = QString::fromUtf8(readFile.readAll());
    }

    const auto lineMatchesPattern = [&](const QString &line) {
        const QString trimmed = line.trimmed();
        return trimmed == pattern || trimmed == normalized;
    };

    for (const QString &line : content.split(QLatin1Char('\n'), Qt::KeepEmptyParts)) {
        if (lineMatchesPattern(line)) {
            GitProcessResult result;
            result.exitCode = 0;
            return result;
        }
    }

    if (!content.isEmpty() && !content.endsWith(QLatin1Char('\n'))) {
        content += QLatin1Char('\n');
    }
    content += pattern + QLatin1Char('\n');

    QFile writeFile(gitignorePath);
    if (!writeFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = QStringLiteral("Could not write .gitignore");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }
    writeFile.write(content.toUtf8());

    const WorkingTreeChange change = changeForPath(repoPath, path);
    if (!change.isUntracked()) {
        QStringList args{QStringLiteral("rm"), QStringLiteral("--cached"), QStringLiteral("-r"),
                         QStringLiteral("--")};
        args.append(pathsForDiff(path));
        const GitProcessResult result = m_runner.run(repoPath, args);
        if (!result.success()) {
            setError(QStringLiteral("git rm --cached failed"), result);
        }
        return result;
    }

    GitProcessResult result;
    result.exitCode = 0;
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

    const QString trimmed = name.trimmed();
    if (isDetachedHeadLabel(trimmed)) {
        m_lastError = QStringLiteral("Not a branch: %1").arg(trimmed);
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const int slash = trimmed.indexOf(QLatin1Char('/'));
    if (slash > 0) {
        const QString remote = trimmed.left(slash);
        if (remotes(repoPath).contains(remote)) {
            const QString localName = trimmed.mid(slash + 1);
            if (branchExists(repoPath, localName)) {
                const GitProcessResult result =
                    m_runner.run(repoPath, {QStringLiteral("checkout"), localName});
                if (!result.success()) {
                    setError(QStringLiteral("git checkout failed"), result);
                }
                return result;
            }

            const GitProcessResult result =
                m_runner.run(repoPath, {QStringLiteral("checkout"), QStringLiteral("-b"), localName,
                                        QStringLiteral("--track"), trimmed});
            if (!result.success()) {
                setError(QStringLiteral("git checkout failed"), result);
            }
            return result;
        }
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("checkout"), trimmed});
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

GitProcessResult GitService::pullBranch(const QString &repoPath,
                                        const QString &remote,
                                        const QString &branchName) const
{
    m_lastError.clear();

    const QString trimmedRemote = remote.trimmed();
    const QString trimmedBranch = branchName.trimmed();
    if (trimmedRemote.isEmpty()) {
        m_lastError = QStringLiteral("Remote name is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    QStringList args{QStringLiteral("pull"), QStringLiteral("--prune"), trimmedRemote};
    if (!trimmedBranch.isEmpty()) {
        args << trimmedBranch;
    }

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git pull failed"), result);
    }
    return result;
}

GitProcessResult GitService::fetchAll(const QString &repoPath) const
{
    m_lastError.clear();

    const GitProcessResult result = m_runner.run(
        repoPath, {QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune")});
    if (!result.success()) {
        setError(QStringLiteral("git fetch failed"), result);
    }
    return result;
}

GitProcessResult GitService::fetchRemote(const QString &repoPath, const QString &remote) const
{
    m_lastError.clear();

    const QString trimmedRemote = remote.trimmed();
    if (trimmedRemote.isEmpty()) {
        m_lastError = QStringLiteral("Remote name is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const GitProcessResult result = m_runner.run(
        repoPath, {QStringLiteral("fetch"), QStringLiteral("--prune"), trimmedRemote});
    if (!result.success()) {
        setError(QStringLiteral("git fetch failed"), result);
    }
    return result;
}

QString GitService::configValue(const QString &repoPath, const QString &key) const
{
    m_lastError.clear();

    const GitProcessResult result = m_runner.run(repoPath, {QStringLiteral("config"), key});
    if (!result.success()) {
        return {};
    }
    return result.stdoutText.trimmed();
}

bool GitService::hasUserIdentity(const QString &repoPath) const
{
    return !configValue(repoPath, QStringLiteral("user.name")).isEmpty()
           && !configValue(repoPath, QStringLiteral("user.email")).isEmpty();
}

bool GitService::isCommitIdentityError(const GitProcessResult &result)
{
    const QString text = result.stderrText + result.stdoutText;
    return text.contains(QStringLiteral("Author identity unknown"))
           || text.contains(QStringLiteral("Please tell me who you are"))
           || text.contains(QStringLiteral("unable to auto-detect email address"));
}

GitProcessResult GitService::setUserIdentity(const QString &repoPath,
                                             const QString &name,
                                             const QString &email,
                                             bool global) const
{
    m_lastError.clear();

    const QString trimmedName = name.trimmed();
    const QString trimmedEmail = email.trimmed();
    if (trimmedName.isEmpty()) {
        m_lastError = QStringLiteral("Author name is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }
    if (trimmedEmail.isEmpty()) {
        m_lastError = QStringLiteral("Author email is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    auto configArgs = [global](const QString &key, const QString &value) {
        QStringList args{QStringLiteral("config")};
        if (global) {
            args << QStringLiteral("--global");
        }
        args << key << value;
        return args;
    };

    const GitProcessResult nameResult =
        m_runner.run(repoPath, configArgs(QStringLiteral("user.name"), trimmedName));
    if (!nameResult.success()) {
        setError(QStringLiteral("git config user.name failed"), nameResult);
        return nameResult;
    }

    const GitProcessResult emailResult =
        m_runner.run(repoPath, configArgs(QStringLiteral("user.email"), trimmedEmail));
    if (!emailResult.success()) {
        setError(QStringLiteral("git config user.email failed"), emailResult);
        return emailResult;
    }

    return emailResult;
}

QString GitService::remoteUrl(const QString &repoPath, const QString &remote, bool pushUrl) const
{
    m_lastError.clear();

    QStringList args{QStringLiteral("remote"), QStringLiteral("get-url")};
    if (pushUrl) {
        args << QStringLiteral("--push");
    }
    args << remote.trimmed();

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.success()) {
        setError(QStringLiteral("git remote get-url failed"), result);
        return {};
    }
    return result.stdoutText.trimmed();
}

bool GitService::isHttpsRemoteUrl(const QString &url)
{
    return url.startsWith(QStringLiteral("https://")) || url.startsWith(QStringLiteral("http://"));
}

bool GitService::isRemoteAuthError(const GitProcessResult &result)
{
    const QString text = result.stderrText + result.stdoutText;
    return text.contains(QStringLiteral("Authentication failed"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("could not read Username"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("could not read Password"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("Invalid username or password"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("Permission denied (publickey)"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("terminal prompts disabled"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("HTTP Basic: Access denied"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("Support for password authentication was removed"),
                            Qt::CaseInsensitive)
           || text.contains(QStringLiteral("access denied"), Qt::CaseInsensitive);
}

GitProcessResult GitService::probeRemote(const QString &repoPath, const QString &remote) const
{
    m_lastError.clear();

    const QString trimmedRemote = remote.trimmed();
    if (trimmedRemote.isEmpty()) {
        m_lastError = QStringLiteral("Remote name is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("ls-remote"), trimmedRemote, QStringLiteral("HEAD")});
    if (!result.success()) {
        setError(QStringLiteral("git ls-remote failed"), result);
    }
    return result;
}

bool GitService::hasRemoteCredentials(const QString &repoPath, const QString &url) const
{
    if (!isHttpsRemoteUrl(url)) {
        return false;
    }

    const QString request = credentialRequestForUrl(url);
    if (request.isEmpty()) {
        return false;
    }

    const GitProcessResult result = m_runner.runWithInput(
        repoPath, {QStringLiteral("credential"), QStringLiteral("fill")}, request.toUtf8());
    if (!result.success()) {
        return false;
    }

    return !credentialPasswordFromOutput(result.stdoutText).isEmpty();
}

GitProcessResult GitService::storeRemoteCredentials(const QString &repoPath,
                                                    const QString &url,
                                                    const QString &username,
                                                    const QString &password) const
{
    m_lastError.clear();

    const QString trimmedUser = username.trimmed();
    const QString trimmedPassword = password.trimmed();
    if (trimmedUser.isEmpty()) {
        m_lastError = QStringLiteral("Username is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }
    if (trimmedPassword.isEmpty()) {
        m_lastError = QStringLiteral("Token is empty");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    const QString request = credentialApproveForUrl(url, trimmedUser, trimmedPassword);
    if (request.isEmpty()) {
        m_lastError = QStringLiteral("Unsupported remote URL");
        GitProcessResult result;
        result.exitCode = 1;
        return result;
    }

    if (configValue(repoPath, QStringLiteral("credential.helper")).isEmpty()) {
        const GitProcessResult helperResult = m_runner.run(
            repoPath,
            {QStringLiteral("config"), QStringLiteral("--global"), QStringLiteral("credential.helper"),
             QStringLiteral("store")});
        if (!helperResult.success()) {
            setError(QStringLiteral("git config credential.helper failed"), helperResult);
            return helperResult;
        }
    }

    const GitProcessResult result = m_runner.runWithInput(
        repoPath, {QStringLiteral("credential"), QStringLiteral("approve")}, request.toUtf8());
    if (!result.success()) {
        setError(QStringLiteral("git credential approve failed"), result);
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
        line = StatusParser::normalizePorcelainLine(line);
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

WorkingTreeChange GitService::changeForPath(const QString &repoPath, const QString &path) const
{
    WorkingTreeChange change;
    const QString normalizedPath = gitPathInRepo(repoPath, path);
    if (normalizedPath.isEmpty()) {
        return change;
    }

    const GitProcessResult result = m_runner.run(
        repoPath, {QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--"),
                   normalizedPath});
    if (!result.success()) {
        return change;
    }

    for (QString line : result.stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        line = StatusParser::normalizePorcelainLine(line);
        WorkingTreeChange parsed;
        if (StatusParser::parsePorcelainLine(line, &parsed) && parsed.path == normalizedPath) {
            return parsed;
        }
    }

    return change;
}

bool GitService::hasCachedDiffForPath(const QString &repoPath, const QString &path) const
{
    const QString normalizedPath = StatusParser::normalizeGitPath(path);
    if (normalizedPath.isEmpty()) {
        return false;
    }

    for (const QString &diffPath : pathsForStagedDiff(m_runner, repoPath, normalizedPath)) {
        const GitProcessResult result =
            m_runner.run(repoPath, {QStringLiteral("diff"), QStringLiteral("--cached"),
                                    QStringLiteral("--quiet"), QStringLiteral("--"), diffPath});
        if (result.exitCode == 1) {
            return true;
        }
    }

    return false;
}

QString GitService::runDiffCommand(const QString &repoPath, const QStringList &args) const
{
    m_lastDiffCommand =
        QStringLiteral("git -C ") + QDir::toNativeSeparators(repoPath) + QLatin1Char(' ')
        + args.join(QLatin1Char(' '));

    const GitProcessResult result = m_runner.run(repoPath, args);
    if (!result.stdoutText.trimmed().isEmpty()) {
        return acceptDiffOutput(result.stdoutText);
    }
    if (!result.diffSucceeded()) {
        setError(QStringLiteral("git diff failed"), result);
    }
    return {};
}

QString GitService::stagedDiffForPath(const QString &repoPath, const QString &path) const
{
    const QString normalizedPath = StatusParser::normalizeGitPath(path);
    if (normalizedPath.isEmpty()) {
        return {};
    }

    const QStringList diffPaths = pathsForStagedDiff(m_runner, repoPath, normalizedPath);

    auto recordCommand = [&](const QStringList &args) {
        m_lastDiffCommand = QStringLiteral("git -C ") + QDir::toNativeSeparators(repoPath)
                            + QLatin1Char(' ') + args.join(QLatin1Char(' '));
    };

    auto tryCachedForPath = [&](const QString &diffPath) -> QString {
        QStringList args{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                         QStringLiteral("-U3"), QStringLiteral("--cached"),
                         QStringLiteral("--"), diffPath};
        recordCommand(args);
        const GitProcessResult result = m_runner.run(repoPath, args);
        if (result.diffSucceeded() && !result.stdoutText.trimmed().isEmpty()) {
            const QString patch = acceptDiffOutput(result.stdoutText);
            if (!patch.isEmpty()) {
                return patch;
            }
            if (m_lastDiffIsBinary) {
                return {};
            }
        }
        m_lastError.clear();
        return {};
    };

    for (const QString &diffPath : diffPaths) {
        const QString patch = tryCachedForPath(diffPath);
        if (!patch.isEmpty()) {
            return patch;
        }
        if (m_lastDiffIsBinary) {
            return {};
        }
    }

    const bool headExists =
        m_runner.run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                                QStringLiteral("HEAD")})
            .success();

    if (headExists) {
        for (const QString &diffPath : diffPaths) {
            QStringList args{QStringLiteral("diff"), QStringLiteral("-U3"),
                             QStringLiteral("HEAD:") + diffPath, QStringLiteral(":0:") + diffPath};
            recordCommand(args);
            const GitProcessResult colon = m_runner.run(repoPath, args);
            if (colon.diffSucceeded() && !colon.stdoutText.trimmed().isEmpty()) {
                const QString patch = acceptDiffOutput(colon.stdoutText);
                if (!patch.isEmpty()) {
                    return patch;
                }
                if (m_lastDiffIsBinary) {
                    return {};
                }
            }
            m_lastError.clear();
            if (m_lastDiffIsBinary) {
                return {};
            }
        }
    }

    QStringList fullArgs{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                         QStringLiteral("-U3"), QStringLiteral("--cached")};
    recordCommand(fullArgs);
    const GitProcessResult full = m_runner.run(repoPath, fullArgs);
    if (full.diffSucceeded() && !full.stdoutText.trimmed().isEmpty()) {
        const QString extracted = DiffParser::extractFilePatch(full.stdoutText, normalizedPath);
        if (!extracted.isEmpty()) {
            return acceptDiffOutput(extracted);
        }
        for (const QString &diffPath : diffPaths) {
            const QString byPath = DiffParser::extractFilePatch(full.stdoutText, diffPath);
            if (!byPath.isEmpty()) {
                return acceptDiffOutput(byPath);
            }
        }
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
    m_lastDiffIsBinary = false;

    const QString normalizedPath = gitPathInRepo(repoPath, path);
    if (normalizedPath.isEmpty()) {
        return {};
    }

    const QStringList diffPaths = pathsForDiff(normalizedPath);

    auto runDiff = [&](const QStringList &extraArgs, const QStringList &paths) -> QString {
        QStringList args{QStringLiteral("diff"), QStringLiteral("--no-ext-diff"),
                         QStringLiteral("--unified=3")};
        args << extraArgs;
        if (!paths.isEmpty()) {
            args << QStringLiteral("--") << paths;
        }
        return runDiffCommand(repoPath, args);
    };

    switch (scope) {
    case WorkingDiffScope::Staged:
        if (!hasCachedDiffForPath(repoPath, normalizedPath)) {
            return {};
        }
        return stagedDiffForPath(repoPath, normalizedPath);
    case WorkingDiffScope::AgainstHead:
        return runDiff({QStringLiteral("HEAD")}, diffPaths);
    case WorkingDiffScope::Unstaged:
        break;
    }

    if (change.isUntracked()) {
        const QString absPath = QDir(repoPath).filePath(diffPaths.front());
        if (isWorktreePathBinary(absPath)) {
            m_lastDiffIsBinary = true;
            return {};
        }

        m_lastDiffCommand.clear();
        QStringList args;
        args << QStringLiteral("diff") << QStringLiteral("--unified=3") << QStringLiteral("--no-index")
             << QStringLiteral("/dev/null") << absPath;

        const GitProcessResult result = m_runner.run(repoPath, args);
        if (!result.diffSucceeded()) {
            setError(QStringLiteral("git diff failed"), result);
            return {};
        }
        return acceptDiffOutput(result.stdoutText);
    }

    return runDiff({}, diffPaths);
}

QString GitService::commitFileDiff(const QString &repoPath,
                                   const QString &hash,
                                   const QString &path) const
{
    m_lastError.clear();
    m_lastDiffIsBinary = false;

    const QString gitPath = gitPathInRepo(repoPath, path);
    if (gitPath.isEmpty()) {
        return {};
    }

    const GitProcessResult result =
        m_runner.run(repoPath, {QStringLiteral("show"), QStringLiteral("--format="),
                                QStringLiteral("--unified=3"), hash, QStringLiteral("--"), gitPath});
    if (!result.success()) {
        setError(QStringLiteral("git show failed"), result);
        return {};
    }

    if (isBinaryDiffOutput(result.stdoutText)
        || result.stderrText.contains(QStringLiteral("Binary files"), Qt::CaseInsensitive)
        || result.stderrText.contains(QStringLiteral("binary file"), Qt::CaseInsensitive)) {
        m_lastDiffIsBinary = true;
        return {};
    }
    return result.stdoutText;
}

std::vector<Commit> GitService::logFileHistory(const QString &repoPath, const QString &path) const
{
    m_lastError.clear();

    const QString gitPath = gitPathInRepo(repoPath, path);
    if (gitPath.isEmpty()) {
        return {};
    }

    const GitProcessResult result = m_runner.run(
        repoPath,
        {QStringLiteral("log"), QStringLiteral("--follow"),
         QStringLiteral("--format=%H%x09%P%x09%an%x09%ad%x09%s"),
         QStringLiteral("--date=iso-strict"), QStringLiteral("--"), gitPath});
    if (!result.success()) {
        setError(QStringLiteral("git log failed"), result);
        return {};
    }

    return LogParser::parseLogOutput(result.stdoutText);
}

WorkingFileContent GitService::workingTreeFileContent(const QString &repoPath,
                                                      const QString &path,
                                                      WorkingDiffScope scope,
                                                      const WorkingTreeChange &change,
                                                      WorkingFileSide side) const
{
    m_lastError.clear();

    WorkingFileContent result;
    const QString normalizedPath = StatusParser::normalizeGitPath(path);
    if (normalizedPath.isEmpty()) {
        result.error = QStringLiteral("File path is empty");
        return result;
    }

    const QStringList diffPaths = pathsForDiff(normalizedPath);
    if (diffPaths.isEmpty()) {
        result.error = QStringLiteral("File path is empty");
        return result;
    }
    const QString gitPath = diffPaths.front();

    const auto headExists = [&]() {
        return m_runner
            .run(repoPath, {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                            QStringLiteral("HEAD")})
            .success();
    };

    const auto readGitSpec = [&](const QString &spec) -> WorkingFileContent {
        WorkingFileContent blob;
        const GitProcessResult showResult =
            m_runner.run(repoPath, {QStringLiteral("show"), spec});
        if (showResult.stderrText.contains(QStringLiteral("Binary files"), Qt::CaseInsensitive)
            || showResult.stderrText.contains(QStringLiteral("binary file"), Qt::CaseInsensitive)) {
            blob.ok = true;
            blob.binary = true;
            return blob;
        }
        if (!showResult.success()) {
            if (showResult.stderrText.contains(QStringLiteral("exists on disk"),
                                               Qt::CaseInsensitive)
                || showResult.stderrText.contains(QStringLiteral("does not exist"),
                                                  Qt::CaseInsensitive)
                || showResult.stderrText.contains(QStringLiteral("bad revision"),
                                                  Qt::CaseInsensitive)) {
                blob.missing = true;
                return blob;
            }
            blob.error = showResult.stderrText.trimmed();
            if (blob.error.isEmpty()) {
                blob.error = QStringLiteral("git show failed");
            }
            return blob;
        }
        if (showResult.stdoutText.contains(QChar::Null)) {
            blob.ok = true;
            blob.binary = true;
            return blob;
        }
        blob.ok = true;
        blob.content = showResult.stdoutText;
        return blob;
    };

    const auto readWorktree = [&]() -> WorkingFileContent {
        WorkingFileContent file;
        const QString absPath = QDir(repoPath).filePath(gitPath);
        QFile io(absPath);
        if (!io.exists()) {
            file.missing = true;
            return file;
        }
        if (!io.open(QIODevice::ReadOnly)) {
            file.error = io.errorString();
            return file;
        }
        const QByteArray bytes = io.readAll();
        if (bytes.contains('\0')) {
            file.ok = true;
            file.binary = true;
            return file;
        }
        file.ok = true;
        file.content = QString::fromUtf8(bytes);
        return file;
    };

    switch (scope) {
    case WorkingDiffScope::Staged:
        if (side == WorkingFileSide::Before) {
            if (!headExists()) {
                result.missing = true;
                return result;
            }
            return readGitSpec(QStringLiteral("HEAD:") + gitPath);
        }
        return readGitSpec(QStringLiteral(":") + gitPath);

    case WorkingDiffScope::AgainstHead:
        if (side == WorkingFileSide::Before) {
            if (!headExists()) {
                result.missing = true;
                return result;
            }
            return readGitSpec(QStringLiteral("HEAD:") + gitPath);
        }
        return readWorktree();

    case WorkingDiffScope::Unstaged:
        if (change.isUntracked()) {
            if (side == WorkingFileSide::Before) {
                result.missing = true;
                return result;
            }
            return readWorktree();
        }
        if (side == WorkingFileSide::Before) {
            const WorkingFileContent index = readGitSpec(QStringLiteral(":") + gitPath);
            if (index.ok || index.binary || !index.error.isEmpty()) {
                return index;
            }
            if (!headExists()) {
                result.missing = true;
                return result;
            }
            return readGitSpec(QStringLiteral("HEAD:") + gitPath);
        }
        return readWorktree();
    }

    result.error = QStringLiteral("Unsupported scope");
    return result;
}

WorkingFileContent GitService::commitFileContent(const QString &repoPath,
                                                 const QString &hash,
                                                 const QString &path,
                                                 WorkingFileSide side) const
{
    m_lastError.clear();

    WorkingFileContent result;
    const QString normalizedPath = StatusParser::normalizeGitPath(path);
    if (normalizedPath.isEmpty() || hash.isEmpty()) {
        result.error = QStringLiteral("File path or commit is empty");
        return result;
    }

    const QString gitPath = pathsForDiff(normalizedPath).value(0);
    if (gitPath.isEmpty()) {
        result.error = QStringLiteral("File path is empty");
        return result;
    }

    const auto readGitSpec = [&](const QString &spec) -> WorkingFileContent {
        WorkingFileContent blob;
        const GitProcessResult showResult =
            m_runner.run(repoPath, {QStringLiteral("show"), spec});
        if (showResult.stderrText.contains(QStringLiteral("Binary files"), Qt::CaseInsensitive)
            || showResult.stderrText.contains(QStringLiteral("binary file"), Qt::CaseInsensitive)) {
            blob.ok = true;
            blob.binary = true;
            return blob;
        }
        if (!showResult.success()) {
            blob.missing = true;
            return blob;
        }
        if (showResult.stdoutText.contains(QChar::Null)) {
            blob.ok = true;
            blob.binary = true;
            return blob;
        }
        blob.ok = true;
        blob.content = showResult.stdoutText;
        return blob;
    };

    if (side == WorkingFileSide::After) {
        return readGitSpec(hash + QLatin1Char(':') + gitPath);
    }

    const GitProcessResult parentResult =
        m_runner.run(repoPath, {QStringLiteral("rev-parse"), hash + QStringLiteral("^")});
    if (!parentResult.success()) {
        result.missing = true;
        return result;
    }

    const QString parentHash = parentResult.stdoutText.trimmed();
    if (parentHash.isEmpty()) {
        result.missing = true;
        return result;
    }

    return readGitSpec(parentHash + QLatin1Char(':') + gitPath);
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
