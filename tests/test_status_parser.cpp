#include "git/StatusParser.h"

#include <QtTest>

class TestStatusParser : public QObject {
    Q_OBJECT

private slots:
    void parsesStagedAddWithTwoSpaces();
    void parsesStagedAddWithOneSpace();
    void parsesUntracked();
    void parsesModifiedInWorkTree();
    void parsesStagedOnlyModified();
    void stripsCarriageReturnFromPath();
    void unquotesPorcelainPath();
    void decodesUtf8OctalQuotedPath();
    void parsesPlainUtf8Path();
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

void TestStatusParser::parsesStagedOnlyModified()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral("M  README.md"), &change));
    QCOMPARE(change.path, QStringLiteral("README.md"));
    QCOMPARE(change.statusLabel(), QStringLiteral("M "));
    QVERIFY(change.hasStaged());
    QVERIFY(!change.hasUnstaged());
}

void TestStatusParser::stripsCarriageReturnFromPath()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral("M  README.md\r"), &change));
    QCOMPARE(change.path, QStringLiteral("README.md"));
}

void TestStatusParser::unquotesPorcelainPath()
{
    WorkingTreeChange change;
    QVERIFY(StatusParser::parsePorcelainLine(QStringLiteral("M  \"My File.txt\""), &change));
    QCOMPARE(change.path, QStringLiteral("My File.txt"));
}

void TestStatusParser::decodesUtf8OctalQuotedPath()
{
    const QString expected = QString::fromUtf8("\xd0\xb5\xd1\x83\xd1\x8b\xd0\xb5", 8);

    WorkingTreeChange change;
    const QString line = QStringLiteral("?? \"\\320\\265\\321\\203\\321\\213\\320\\265\"");
    QVERIFY(StatusParser::parsePorcelainLine(line, &change));
    QCOMPARE(change.path, expected);
}

void TestStatusParser::parsesPlainUtf8Path()
{
    const QString expected = QString::fromUtf8("\xd0\xb5\xd1\x83\xd1\x8b\xd0\xb5", 8);

    WorkingTreeChange change;
    const QString line = QStringLiteral("?? ") + expected;
    QVERIFY(StatusParser::parsePorcelainLine(line, &change));
    QCOMPARE(change.path, expected);
}

QTEST_MAIN(TestStatusParser)
#include "test_status_parser.moc"
