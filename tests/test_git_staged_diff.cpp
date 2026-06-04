#include "core/WorkingTreeChange.h"
#include "git/GitProcessRunner.h"
#include "git/GitService.h"
#include "git/StatusParser.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class TestGitStagedDiff : public QObject {
    Q_OBJECT

private slots:
    void parsesStagedOnlyPorcelain();
    void stagedDiffForStagedOnlyFile();
    void stagedDiffBeforeFirstCommit();
    void validateBranchNameAcceptsFeatureBranch();
};

void TestGitStagedDiff::parsesStagedOnlyPorcelain()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral("M  README.md"), &change));
    QCOMPARE(change.path, QStringLiteral("README.md"));
    QCOMPARE(change.statusLabel(), QStringLiteral("M "));
    QVERIFY(change.hasStaged());
    QVERIFY(!change.hasUnstaged());
}

void TestGitStagedDiff::stagedDiffForStagedOnlyFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString repo = temp.path();

    GitProcessRunner runner;
    auto run = [&](const QStringList &args) {
        const GitProcessResult result = runner.run(repo, args);
        QVERIFY2(result.success(), qPrintable(result.stderrText));
    };

    run({QStringLiteral("init")});
    run({QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("t@t")});
    run({QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("t")});

    QFile readme(repo + QStringLiteral("/README.md"));
    QVERIFY(readme.open(QIODevice::WriteOnly | QIODevice::Text));
    readme.write("line1\n");
    readme.close();

    run({QStringLiteral("add"), QStringLiteral("README.md")});
    run({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("init")});

    QVERIFY(readme.open(QIODevice::Append | QIODevice::Text));
    readme.write("line2\n");
    readme.close();

    run({QStringLiteral("add"), QStringLiteral("README.md")});

    WorkingTreeChange change;
    const GitProcessResult status =
        runner.run(repo, {QStringLiteral("status"), QStringLiteral("--porcelain")});
    QVERIFY(status.success());
    const QString line = status.stdoutText.trimmed().split(QLatin1Char('\n')).constFirst();
    QVERIFY(StatusParser::parsePorcelainLine(line, &change));

    GitService git;
    const QString patch =
        git.workingTreeFileDiff(repo, QStringLiteral("README.md"), WorkingDiffScope::Staged, change);
    QVERIFY2(!patch.isEmpty(), qPrintable(git.lastError() + QLatin1Char(' ') + git.lastDiffCommand()));
    QVERIFY(patch.contains(QStringLiteral("diff --git")));
    QVERIFY(patch.contains(QStringLiteral("+line2")));
}

void TestGitStagedDiff::stagedDiffBeforeFirstCommit()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString repo = temp.path();

    GitProcessRunner runner;
    auto run = [&](const QStringList &args) {
        const GitProcessResult result = runner.run(repo, args);
        QVERIFY2(result.success(), qPrintable(result.stderrText));
    };

    run({QStringLiteral("init")});

    QFile readme(repo + QStringLiteral("/README.md"));
    QVERIFY(readme.open(QIODevice::WriteOnly | QIODevice::Text));
    readme.write("new\n");
    readme.close();

    run({QStringLiteral("add"), QStringLiteral("README.md")});

    WorkingTreeChange change;
    change.path = QStringLiteral("README.md");
    change.indexStatus = QLatin1Char('A');
    change.workTreeStatus = QLatin1Char(' ');

    GitService git;
    const QString patch =
        git.workingTreeFileDiff(repo, QStringLiteral("README.md"), WorkingDiffScope::Staged, change);
    QVERIFY2(!patch.isEmpty(), qPrintable(git.lastError() + QLatin1Char(' ') + git.lastDiffCommand()));
    QVERIFY(patch.contains(QStringLiteral("diff --git")));
}

void TestGitStagedDiff::validateBranchNameAcceptsFeatureBranch()
{
    GitService git;
    QVERIFY(git.validateBranchName(QStringLiteral("/tmp"), QStringLiteral("feature/test")).isEmpty());
    QVERIFY(!git.validateBranchName(QStringLiteral("/tmp"), QStringLiteral("bad..name")).isEmpty());
}

QTEST_MAIN(TestGitStagedDiff)
#include "test_git_staged_diff.moc"
