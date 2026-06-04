#include "git/LogParser.h"

#include <QtTest>

class TestLogParser : public QObject {
    Q_OBJECT

private slots:
    void parsesSingleCommit();
    void parsesMergeCommit();
    void skipsMalformedLines();
};

void TestLogParser::parsesSingleCommit()
{
    const QString output =
        QStringLiteral("abc123\tdef456\tAlice\t2024-01-01T12:00:00+00:00\tInitial commit\n");

    const std::vector<Commit> commits = LogParser::parseLogOutput(output);
    QCOMPARE(commits.size(), size_t(1));
    QCOMPARE(commits[0].hash, QStringLiteral("abc123"));
    QCOMPARE(commits[0].parentHashes.size(), 1);
    QCOMPARE(commits[0].parentHashes[0], QStringLiteral("def456"));
    QCOMPARE(commits[0].author, QStringLiteral("Alice"));
    QCOMPARE(commits[0].subject, QStringLiteral("Initial commit"));
}

void TestLogParser::parsesMergeCommit()
{
    const QString output = QStringLiteral(
        "merge1\tparent1 parent2\tBob\t2024-02-01T12:00:00+00:00\tMerge branch 'feature'\n");

    const std::vector<Commit> commits = LogParser::parseLogOutput(output);
    QCOMPARE(commits.size(), size_t(1));
    QVERIFY(commits[0].isMerge());
    QCOMPARE(commits[0].parentHashes.size(), 2);
}

void TestLogParser::skipsMalformedLines()
{
    const QString output = QStringLiteral("incomplete\tline\n"
                                          "full\tparent\tAuthor\t2024-01-01T00:00:00+00:00\tOK\n");

    const std::vector<Commit> commits = LogParser::parseLogOutput(output);
    QCOMPARE(commits.size(), size_t(1));
    QCOMPARE(commits[0].subject, QStringLiteral("OK"));
}

QTEST_MAIN(TestLogParser)
#include "test_log_parser.moc"
