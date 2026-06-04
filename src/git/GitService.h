#pragma once

#include "core/Branch.h"
#include "core/Commit.h"
#include "core/CommitDetails.h"
#include "core/WorkingTreeChange.h"
#include "git/GitProcessRunner.h"

#include <QString>
#include <vector>

struct GitError {
    QString message;
};

enum class WorkingDiffScope {
    Unstaged,
    Staged,
    AgainstHead,
};

class GitService {
public:
    explicit GitService(GitProcessRunner runner = {});

    bool isRepository(const QString &path) const;
    QString discoverGitDir(const QString &path) const;

    std::vector<Commit> logAll(const QString &repoPath, int maxCount = 500) const;
    std::vector<Commit> logBranch(const QString &repoPath,
                                  const QString &branch,
                                  int maxCount = 500) const;
    std::vector<Branch> branches(const QString &repoPath) const;
    QString currentBranch(const QString &repoPath) const;
    bool hasUncommittedChanges(const QString &repoPath) const;
    bool hasStagedChanges(const QString &repoPath) const;

    GitProcessResult stageAll(const QString &repoPath) const;
    GitProcessResult commit(const QString &repoPath, const QString &message) const;

    bool branchExists(const QString &repoPath, const QString &name) const;
    QString validateBranchName(const QString &repoPath, const QString &name) const;
    GitProcessResult createBranch(const QString &repoPath,
                                  const QString &name,
                                  const QString &startPoint = {}) const;
    GitProcessResult checkoutBranch(const QString &repoPath, const QString &name) const;
    GitProcessResult createBranchAndCheckout(const QString &repoPath,
                                               const QString &name,
                                               const QString &startPoint = {}) const;

    GitProcessResult merge(const QString &repoPath,
                           const QString &branch,
                           bool noFf = false) const;
    GitProcessResult mergeAbort(const QString &repoPath) const;
    QStringList unmergedFiles(const QString &repoPath) const;

    CommitDetails commitDetails(const QString &repoPath, const QString &hash) const;
    QString commitFileDiff(const QString &repoPath,
                           const QString &hash,
                           const QString &path) const;

    std::vector<WorkingTreeChange> workingTreeChanges(const QString &repoPath) const;
    QString workingTreeFileDiff(const QString &repoPath,
                                const QString &path,
                                WorkingDiffScope scope,
                                const WorkingTreeChange &change) const;

    QString lastError() const { return m_lastError; }
    QString lastDiffCommand() const { return m_lastDiffCommand; }

private:
    QString stagedDiffForPath(const QString &repoPath, const QString &path) const;
    QString runDiffCommand(const QString &repoPath, const QStringList &args) const;

    mutable QString m_lastDiffCommand;

    std::vector<Commit> runLog(const QString &repoPath,
                               const QStringList &extraArgs,
                               int maxCount) const;
    void setError(const QString &message, const GitProcessResult &result) const;

    GitProcessRunner m_runner;
    mutable QString m_lastError;
};
