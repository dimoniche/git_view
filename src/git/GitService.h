#pragma once

#include "core/Branch.h"
#include "core/Commit.h"
#include "core/CommitDetails.h"
#include "git/GitProcessRunner.h"

#include <QString>
#include <vector>

struct GitError {
    QString message;
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

    GitProcessResult merge(const QString &repoPath,
                           const QString &branch,
                           bool noFf = false) const;
    GitProcessResult mergeAbort(const QString &repoPath) const;
    QStringList unmergedFiles(const QString &repoPath) const;

    CommitDetails commitDetails(const QString &repoPath, const QString &hash) const;

    QString lastError() const { return m_lastError; }

private:
    std::vector<Commit> runLog(const QString &repoPath,
                               const QStringList &extraArgs,
                               int maxCount) const;
    void setError(const QString &message, const GitProcessResult &result) const;

    GitProcessRunner m_runner;
    mutable QString m_lastError;
};
