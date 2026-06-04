#include "git/DiffParser.h"

#include <QtTest>

class TestDiffParser : public QObject {
    Q_OBJECT

private slots:
    void extractsSingleFilePatch();
};

void TestDiffParser::extractsSingleFilePatch()
{
    const QString full = QStringLiteral(
        "diff --git a/README.md b/README.md\n"
        "index 111..222\n"
        "--- a/README.md\n"
        "+++ b/README.md\n"
        "@@ -1 +1 @@\n"
        "-old\n"
        "+new\n"
        "diff --git a/other.txt b/other.txt\n"
        "--- a/other.txt\n");

    const QString patch = DiffParser::extractFilePatch(full, QStringLiteral("README.md"));
    QVERIFY(patch.contains(QStringLiteral("README.md")));
    QVERIFY(patch.contains(QStringLiteral("+new")));
    QVERIFY(!patch.contains(QStringLiteral("other.txt")));
}

QTEST_MAIN(TestDiffParser)
#include "test_diff_parser.moc"
