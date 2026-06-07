#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace DiffParser {

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
    QSet<int> beforePaddingRows;
    QSet<int> afterPaddingRows;
    QSet<int> beforeChangedRows;
    QSet<int> afterChangedRows;
};

QVector<DiffLineMap> buildDiffLineMap(const QString &diff);
FileLineAlignment buildFileLineAlignment(const QString &before, const QString &after);
void augmentFileLineAlignmentFromDiff(FileLineAlignment *alignment,
                                      const QVector<DiffLineMap> &diffMap);
AlignedSideBySideView buildAlignedSideBySideView(const QString &before, const QString &after);
void applyDiffHighlightsToAlignedView(AlignedSideBySideView *view,
                                      const QVector<DiffLineMap> &diffMap);

QString extractFilePatch(const QString &fullPatch, const QString &path);
bool diffHeaderMatchesPath(const QString &diffHeaderLine, const QString &path);

} // namespace DiffParser
