#include "git/StatusParser.h"

#include <QtTest>

class TestStatusParser : public QObject {
    Q_OBJECT

private slots:
    void parsesStagedAddWithTwoSpaces();
    void parsesStagedAddWithOneSpace();
    void parsesUntracked();
    void parsesModifiedInWorkTree();
    void unquotesPorcelainPath();
};

void TestStatusParser::parsesStagedAddWithTwoSpaces()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral("A  Alpha.txt"), &change));
    QCOMPARE(change.path, QStringLiteral("Alpha.txt"));
}

void TestStatusParser::parsesStagedAddWithOneSpace()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral("A Alpha.txt"), &change));
    QCOMPARE(change.path, QStringLiteral("Alpha.txt"));
}

void TestStatusParser::parsesUntracked()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral("?? Zebra.txt"), &change));
    QCOMPARE(change.path, QStringLiteral("Zebra.txt"));
}

void TestStatusParser::parsesModifiedInWorkTree()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral(" M Beta.txt"), &change));
    QCOMPARE(change.path, QStringLiteral("Beta.txt"));
}

void TestStatusParser::unquotesPorcelainPath()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral("M  \"My File.txt\""), &change));
    QCOMPARE(change.path, QStringLiteral("My File.txt"));
}

QTEST_MAIN(TestStatusParser)
#include "test_status_parser.moc"
