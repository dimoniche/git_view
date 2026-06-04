#include "core/WorkingTreeChange.h"
#include "git/GitService.h"
#include "git/StatusParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QtTest>

class TestLiveStaged : public QObject {
    Q_OBJECT

private slots:
    void readmeStagedInGitViewRepo();
};

void TestLiveStaged::readmeStagedInGitViewRepo()
{
    const QString repo = qEnvironmentVariable("GIT_VIEW_LIVE_REPO");
    if (repo.isEmpty()) {
        QSKIP("Set GIT_VIEW_LIVE_REPO to run");
    }

    GitService git;
    const auto changes = git.workingTreeChanges(repo);
    bool found = false;
    for (const WorkingTreeChange &change : changes) {
        if (!change.path.endsWith(QStringLiteral("README.md"))) {
            continue;
        }
        found = true;
        QCOMPARE(change.statusLabel(), QStringLiteral("M "));
        QVERIFY(change.hasStaged());

        const QString patch =
            git.workingTreeFileDiff(repo, change.path, WorkingDiffScope::Staged, change);
        QVERIFY2(!patch.isEmpty(), qPrintable(git.lastError() + QLatin1Char('\n')
                                              + git.lastDiffCommand()));
        QVERIFY(patch.contains(QStringLiteral("diff --git")));
        QVERIFY(patch.contains(QLatin1Char('+')) || patch.contains(QLatin1Char('-')));
    }
    QVERIFY2(found, "README.md not in working tree changes");
}

QTEST_MAIN(TestLiveStaged)
#include "test_live_staged.moc"
