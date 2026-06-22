#pragma once

#include "core/Branch.h"
#include "core/Commit.h"
#include "core/Tag.h"
#include "core/GitRemote.h"
#include "core/CommitDetails.h"
#include "core/WorkingTreeChange.h"
#include "git/GitProcessRunner.h"

#include <QString>
#include <vector>

struct GitError {
    QString message;
};

struct BranchSyncCounts {
    int ahead = 0;
    int behind = 0;
    QString upstream;
    bool valid = false;
};

enum class WorkingDiffScope {
    Unstaged,
    Staged,
    AgainstHead,
};

enum class WorkingFileSide {
    Before,
    After,
};

struct WorkingFileContent {
    bool ok = false;
    bool missing = false;
    bool binary = false;
    QString content;
    QString error;
};

enum class UndoCommitMode {
    KeepStaged,
    KeepUnstaged,
    Discard,
};

class GitService {
public:
    explicit GitService(GitProcessRunner runner = {});

    bool isRepository(const QString &path) const;
    QString discoverGitDir(const QString &path) const;
    GitProcessResult initRepository(const QString &path,
                                    const QString &initialBranch = {}) const;

    std::vector<Commit> logAll(const QString &repoPath, int maxCount = 500) const;
    std::vector<Commit> logBranch(const QString &repoPath,
                                  const QString &branch,
                                  int maxCount = 500) const;
    int commitCount(const QString &repoPath, const QString &branch = {}) const;
    std::vector<Branch> branches(const QString &repoPath) const;
    std::vector<Tag> tags(const QString &repoPath) const;
    QString currentBranch(const QString &repoPath) const;
    QString displayBranchName(const QString &repoPath) const;
    static bool isPseudoDetachedBranchName(const QString &name);
    BranchSyncCounts currentBranchSyncCounts(const QString &repoPath) const;
    bool hasUncommittedChanges(const QString &repoPath) const;
    bool hasStagedChanges(const QString &repoPath) const;

    GitProcessResult stageAll(const QString &repoPath) const;
    GitProcessResult stagePaths(const QString &repoPath, const QStringList &paths) const;
    GitProcessResult unstagePaths(const QString &repoPath, const QStringList &paths) const;
    QString configValue(const QString &repoPath, const QString &key) const;
    bool hasUserIdentity(const QString &repoPath) const;
    static bool isCommitIdentityError(const GitProcessResult &result);
    GitProcessResult setUserIdentity(const QString &repoPath,
                                     const QString &name,
                                     const QString &email,
                                     bool global) const;
    QString remoteUrl(const QString &repoPath, const QString &remote, bool pushUrl = false) const;
    static bool isHttpsRemoteUrl(const QString &url);
    static bool isRemoteAuthError(const GitProcessResult &result);
    GitProcessResult probeRemote(const QString &repoPath, const QString &remote) const;
    bool hasRemoteCredentials(const QString &repoPath, const QString &url) const;
    GitProcessResult storeRemoteCredentials(const QString &repoPath,
                                            const QString &url,
                                            const QString &username,
                                            const QString &password) const;
    GitProcessResult commit(const QString &repoPath, const QString &message) const;
    GitProcessResult amendCommit(const QString &repoPath,
                                 const QString &message,
                                 bool noEdit = false) const;
    bool hasCommits(const QString &repoPath) const;
    bool hasParentCommit(const QString &repoPath) const;
    GitProcessResult undoLastCommit(const QString &repoPath, UndoCommitMode mode) const;
    QString headCommitMessage(const QString &repoPath) const;
    GitProcessResult discardAllChanges(const QString &repoPath) const;
    GitProcessResult discardFileChanges(const QString &repoPath,
                                      const WorkingTreeChange &change) const;
    GitProcessResult addToGitignore(const QString &repoPath, const QString &path) const;

    bool branchExists(const QString &repoPath, const QString &name) const;
    QString validateBranchName(const QString &repoPath, const QString &name) const;
    GitProcessResult createBranch(const QString &repoPath,
                                  const QString &name,
                                  const QString &startPoint = {}) const;
    GitProcessResult checkoutBranch(const QString &repoPath, const QString &name) const;
    GitProcessResult createBranchAndCheckout(const QString &repoPath,
                                               const QString &name,
                                               const QString &startPoint = {}) const;
    GitProcessResult deleteBranch(const QString &repoPath,
                                  const QString &branchName,
                                  bool isRemote,
                                  bool force = false) const;
    QStringList remotes(const QString &repoPath) const;
    std::vector<GitRemote> listRemotes(const QString &repoPath) const;
    QString defaultRemote(const QString &repoPath) const;
    QString validateRemoteName(const QString &name) const;
    GitProcessResult addRemote(const QString &repoPath,
                               const QString &name,
                               const QString &url) const;
    GitProcessResult removeRemote(const QString &repoPath, const QString &name) const;
    GitProcessResult setRemoteUrl(const QString &repoPath,
                                  const QString &name,
                                  const QString &url,
                                  bool push = false) const;
    GitProcessResult publishBranch(const QString &repoPath,
                                   const QString &branchName,
                                   const QString &remote) const;
    GitProcessResult pushBranch(const QString &repoPath,
                                const QString &branchName,
                                const QString &remote,
                                bool forceWithLease = false) const;
    GitProcessResult pullBranch(const QString &repoPath,
                                const QString &remote,
                                const QString &branchName) const;
    GitProcessResult fetchAll(const QString &repoPath) const;
    GitProcessResult fetchRemote(const QString &repoPath, const QString &remote) const;

    GitProcessResult merge(const QString &repoPath,
                           const QString &branch,
                           bool noFf = false) const;
    GitProcessResult mergeAbort(const QString &repoPath) const;
    QStringList unmergedFiles(const QString &repoPath) const;

    CommitDetails commitDetails(const QString &repoPath, const QString &hash) const;
    QString commitFileDiff(const QString &repoPath,
                           const QString &hash,
                           const QString &path) const;
    std::vector<Commit> logFileHistory(const QString &repoPath, const QString &path) const;

    std::vector<WorkingTreeChange> workingTreeChanges(const QString &repoPath) const;
    WorkingTreeChange changeForPath(const QString &repoPath, const QString &path) const;
    bool hasCachedDiffForPath(const QString &repoPath, const QString &path) const;
    QString workingTreeFileDiff(const QString &repoPath,
                                const QString &path,
                                WorkingDiffScope scope,
                                const WorkingTreeChange &change) const;
    WorkingFileContent workingTreeFileContent(const QString &repoPath,
                                              const QString &path,
                                              WorkingDiffScope scope,
                                              const WorkingTreeChange &change,
                                              WorkingFileSide side) const;
    WorkingFileContent commitFileContent(const QString &repoPath,
                                         const QString &hash,
                                         const QString &path,
                                         WorkingFileSide side) const;

    QString lastError() const { return m_lastError; }
    QString lastDiffCommand() const { return m_lastDiffCommand; }
    bool lastDiffWasBinary() const { return m_lastDiffIsBinary; }

private:
    QString stagedDiffForPath(const QString &repoPath, const QString &path) const;
    QString runDiffCommand(const QString &repoPath, const QStringList &args) const;
    QString acceptDiffOutput(const QString &diff) const;
    static bool isBinaryDiffOutput(const QString &text);
    static bool isWorktreePathBinary(const QString &absolutePath);

    mutable QString m_lastDiffCommand;
    mutable bool m_lastDiffIsBinary = false;

    std::vector<Commit> runLog(const QString &repoPath,
                               const QStringList &extraArgs,
                               int maxCount) const;
    void setError(const QString &message, const GitProcessResult &result) const;

    GitProcessRunner m_runner;
    mutable QString m_lastError;
};
