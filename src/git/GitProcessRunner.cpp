#include "git/GitProcessRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

QString GitProcessRunner::findGitExecutable()
{
    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("git"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
    for (const QString &candidate :
         {QStringLiteral("/usr/bin/git"), QStringLiteral("/usr/local/bin/git")}) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QStringLiteral("git");
}

GitProcessResult GitProcessRunner::run(const QString &repoPath,
                                       const QStringList &args,
                                       int timeoutMs) const
{
    GitProcessResult result;

    QStringList command;
    if (!repoPath.isEmpty()) {
        command << QStringLiteral("-C") << repoPath;
    }
    command << args;

    QProcess process;
    process.setProgram(findGitExecutable());
    process.setArguments(command);
    process.setProcessChannelMode(QProcess::SeparateChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    env.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    process.setProcessEnvironment(env);

    process.start();
    if (!process.waitForStarted()) {
        result.stderrText =
            QStringLiteral("Failed to start git: %1").arg(process.errorString());
        return result;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(5000);
        result.stderrText = QStringLiteral("git timed out");
        result.exitCode = -1;
        return result;
    }

    result.exitCode = process.exitCode();
    result.stdoutText = QString::fromUtf8(process.readAllStandardOutput());
    result.stderrText = QString::fromUtf8(process.readAllStandardError());
    return result;
}
