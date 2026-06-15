#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace DiffParser {

constexpr int kLargeTextChars = 300'000;
constexpr int kLargeDiffLines = 2'000;
constexpr int kMaxAlignmentCells = 4'000'000;

struct DiffLineMap {
    int oldLine = -1;
    int newLine = -1;
    bool isHunkHeader = false;
};

struct FileLineAlignment {
    int oldLineCount = 0;
    int newLineCount = 0;
    QVector<int> oldToNew;
    QVector<int> newToOld;
};

struct AlignedSideBySideView {
    QString beforeText;
    QString afterText;
    QHash<int, int> beforeSourceToDisplay;
    QHash<int, int> afterSourceToDisplay;
    QVector<int> diffLineToDisplayRow;
    QSet<int> beforePaddingRows;
    QSet<int> afterPaddingRows;
    QSet<int> beforeChangedRows;
    QSet<int> afterChangedRows;
};

QVector<DiffLineMap> buildDiffLineMap(const QString &diff);
FileLineAlignment buildFileLineAlignment(const QString &before, const QString &after);
void augmentFileLineAlignmentFromDiff(FileLineAlignment *alignment,
                                      const QVector<DiffLineMap> &diffMap);
AlignedSideBySideView buildAlignedSideBySideView(const QString &before, const QString &after,
                                                const QVector<DiffLineMap> &diffMap);
void applyDiffHighlightsToAlignedView(AlignedSideBySideView *view,
                                      const QVector<DiffLineMap> &diffMap);

QString extractFilePatch(const QString &fullPatch, const QString &path);
bool diffHeaderMatchesPath(const QString &diffHeaderLine, const QString &path);

QString normalizeLineContent(const QString &line);
QString normalizeForContentComparison(const QString &text);
bool lineContentEqual(const QString &a, const QString &b);
bool fileContentsEquivalent(const QString &before, const QString &after);
bool diffShowsNoContentChange(const QString &diff);
QString sanitizeDiffLineEndingChanges(const QString &diff);
bool isLargeTextContent(const QString &text);
bool isLargeDiff(const QString &diff);
bool shouldSkipExpensiveDiffProcessing(const QString &diff,
                                     const QString &before = {},
                                     const QString &after = {});
QString prepareDiffForDisplay(const QString &diff,
                              const QString &before = {},
                              const QString &after = {});

} // namespace DiffParser
