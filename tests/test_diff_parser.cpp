#include "git/DiffParser.h"

#include <QtTest>

class TestDiffParser : public QObject {
    Q_OBJECT

private slots:
    void extractsSingleFilePatch();
    void alignedViewHandlesUnequalGapBetweenHunks();
    void alignedViewMapsAllHunkChangesToDisplayRows();
};

void TestDiffParser::alignedViewHandlesUnequalGapBetweenHunks()
{
    QStringList beforeLines;
    QStringList afterLines;
    for (int i = 1; i <= 20; ++i) {
        beforeLines.append(QStringLiteral("old-%1").arg(i));
    }
    for (int i = 1; i <= 20; ++i) {
        if (i == 13 || i == 14) {
            continue;
        }
        afterLines.append(QStringLiteral("new-%1").arg(i));
    }

    const QString diff = QStringLiteral(
        "@@ -5,3 +5,3 @@\n"
        " old-5\n"
        " old-6\n"
        " old-7\n"
        "@@ -15,3 +13,3 @@\n"
        " old-15\n"
        " old-16\n"
        " old-17\n");

    const QVector<DiffParser::DiffLineMap> map = DiffParser::buildDiffLineMap(diff);
    const DiffParser::AlignedSideBySideView view =
        DiffParser::buildAlignedSideBySideView(beforeLines.join(QLatin1Char('\n')),
                                               afterLines.join(QLatin1Char('\n')), map);

    QCOMPARE(view.beforeText.split(QLatin1Char('\n')).size(),
             view.afterText.split(QLatin1Char('\n')).size());

    const int rowOld13 = view.beforeSourceToDisplay.value(13);
    const int rowOld14 = view.beforeSourceToDisplay.value(14);
    const int rowOld15 = view.beforeSourceToDisplay.value(15);
    const int rowNew13 = view.afterSourceToDisplay.value(13);

    QVERIFY(rowOld13 > 0);
    QVERIFY(rowOld14 > 0);
    QVERIFY(rowOld15 > 0);
    QVERIFY(rowNew13 > 0);
    QCOMPARE(rowOld15, rowNew13);
    QVERIFY(view.afterPaddingRows.contains(rowOld13));
    QVERIFY(view.afterPaddingRows.contains(rowOld14));

    int diffLineIndexForOld15 = -1;
    for (int i = 0; i < map.size(); ++i) {
        if (!map.at(i).isHunkHeader && map.at(i).oldLine == 15 && map.at(i).newLine == 13) {
            diffLineIndexForOld15 = i;
            break;
        }
    }
    QVERIFY(diffLineIndexForOld15 >= 0);
    QCOMPARE(view.diffLineToDisplayRow.at(diffLineIndexForOld15), rowOld15);
}

void TestDiffParser::alignedViewMapsAllHunkChangesToDisplayRows()
{
    QStringList beforeLines;
    QStringList afterLines;
    for (int i = 1; i <= 50; ++i) {
        beforeLines.append(QStringLiteral("line-%1").arg(i));
    }
    for (int i = 1; i <= 50; ++i) {
        if (i == 4) {
            afterLines.append(QStringLiteral("line-4-modified"));
        } else if (i == 21) {
            afterLines.append(QStringLiteral("line-21-modified"));
        } else if (i == 41) {
            afterLines.append(QStringLiteral("line-41-modified"));
        } else {
            afterLines.append(QStringLiteral("line-%1").arg(i));
        }
    }

    const QString diff = QStringLiteral(
        "@@ -3,2 +3,2 @@\n"
        " line-3\n"
        "-line-4\n"
        "+line-4-modified\n"
        "@@ -20,2 +20,2 @@\n"
        " line-20\n"
        "-line-21\n"
        "+line-21-modified\n"
        "@@ -40,2 +40,2 @@\n"
        " line-40\n"
        "-line-41\n"
        "+line-41-modified\n");

    const QVector<DiffParser::DiffLineMap> map = DiffParser::buildDiffLineMap(diff);
    const DiffParser::AlignedSideBySideView view =
        DiffParser::buildAlignedSideBySideView(beforeLines.join(QLatin1Char('\n')),
                                               afterLines.join(QLatin1Char('\n')), map);

    const auto expectDiffLineNavigatesTo = [&](int oldLine, int newLine) {
        int diffIndex = -1;
        for (int i = 0; i < map.size(); ++i) {
            const DiffParser::DiffLineMap &entry = map.at(i);
            if (!entry.isHunkHeader && entry.oldLine == oldLine && entry.newLine == newLine) {
                diffIndex = i;
                break;
            }
        }
        QVERIFY2(diffIndex >= 0, qPrintable(QStringLiteral("missing diff entry %1/%2")
                                               .arg(oldLine)
                                               .arg(newLine)));
        int expectedRow = 0;
        if (oldLine > 0) {
            expectedRow = view.beforeSourceToDisplay.value(oldLine, 0);
        } else if (newLine > 0) {
            expectedRow = view.afterSourceToDisplay.value(newLine, 0);
        }
        QVERIFY2(expectedRow > 0,
                 qPrintable(QStringLiteral("missing display row for %1/%2").arg(oldLine).arg(newLine)));
        QCOMPARE(view.diffLineToDisplayRow.at(diffIndex), expectedRow);
    };

    expectDiffLineNavigatesTo(4, -1);   // deletion in first hunk
    expectDiffLineNavigatesTo(-1, 4);   // insertion in first hunk
    expectDiffLineNavigatesTo(21, -1);  // deletion in second hunk
    expectDiffLineNavigatesTo(-1, 21);  // insertion in second hunk
    expectDiffLineNavigatesTo(41, -1);  // deletion in third hunk
    expectDiffLineNavigatesTo(-1, 41);  // insertion in third hunk
}

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
