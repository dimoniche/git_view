#pragma once

#include <QString>
#include <QStringList>

struct GitProcessResult {
    int exitCode = -1;
    QString stdoutText;
    QString stderrText;
    bool success() const { return exitCode == 0; }
};

class GitProcessRunner {
public:
    static QString findGitExecutable();

    GitProcessResult run(const QString &repoPath,
                         const QStringList &args,
                         int timeoutMs = 120000) const;
};
