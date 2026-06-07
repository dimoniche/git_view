#include "git/DiffParser.h"

#include <QStringList>
#include <QRegularExpression>
#include <QSet>

namespace DiffParser {

bool diffHeaderMatchesPath(const QString &diffHeaderLine, const QString &path)
{
    if (!diffHeaderLine.startsWith(QStringLiteral("diff --git ")) || path.isEmpty()) {
        return false;
    }

    const QString quoted = QStringLiteral("/") + path;
    return diffHeaderLine.contains(QStringLiteral(" a") + quoted)
           || diffHeaderLine.contains(QStringLiteral(" b") + quoted)
           || diffHeaderLine.contains(QStringLiteral(" a/") + path)
           || diffHeaderLine.contains(QStringLiteral(" b/") + path);
}

QString extractFilePatch(const QString &fullPatch, const QString &path)
{
    if (fullPatch.isEmpty() || path.isEmpty()) {
        return {};
    }

    const QStringList lines = fullPatch.split(QLatin1Char('\n'));
    QStringList chunk;
    bool capturing = false;

    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("diff --git "))) {
            if (capturing) {
                break;
            }
            capturing = diffHeaderMatchesPath(line, path);
            if (capturing) {
                chunk.clear();
                chunk.append(line);
            }
            continue;
        }
        if (capturing) {
            chunk.append(line);
        }
    }

    return chunk.join(QLatin1Char('\n'));
}

QVector<DiffLineMap> buildDiffLineMap(const QString &diff)
{
    QVector<DiffLineMap> map;
    if (diff.isEmpty()) {
        return map;
    }

    static const QRegularExpression hunkRe(
        QStringLiteral("^@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@"));

    int oldLine = 0;
    int newLine = 0;

    const QStringList lines = diff.split(QLatin1Char('\n'));
    map.reserve(lines.size());

    for (const QString &rawLine : lines) {
        const QString line = rawLine.endsWith(QLatin1Char('\r'))
                                 ? rawLine.chopped(1)
                                 : rawLine;
        DiffLineMap entry;

        const QRegularExpressionMatch hunkMatch = hunkRe.match(line);
        if (hunkMatch.hasMatch()) {
            oldLine = hunkMatch.captured(1).toInt();
            newLine = hunkMatch.captured(3).toInt();
            entry.oldLine = oldLine;
            entry.newLine = newLine;
            entry.isHunkHeader = true;
            map.append(entry);
            continue;
        }

        if (line.startsWith(QLatin1Char('-')) && !line.startsWith(QStringLiteral("---"))) {
            entry.oldLine = oldLine;
            ++oldLine;
            map.append(entry);
            continue;
        }

        if (line.startsWith(QLatin1Char('+')) && !line.startsWith(QStringLiteral("+++"))) {
            entry.newLine = newLine;
            ++newLine;
            map.append(entry);
            continue;
        }

        if (line.startsWith(QLatin1Char(' '))) {
            entry.oldLine = oldLine;
            entry.newLine = newLine;
            ++oldLine;
            ++newLine;
            map.append(entry);
            continue;
        }

        map.append(entry);
    }

    return map;
}

namespace {

QStringList splitSourceLines(const QString &text)
{
    if (text.isEmpty()) {
        return {};
    }

    QStringList lines = text.split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }
    return lines;
}

void fillOldToNewGaps(QVector<int> *mapping, int lineCount)
{
    int lastMapped = 1;
    for (int line = 1; line <= lineCount; ++line) {
        if ((*mapping)[line] > 0) {
            lastMapped = (*mapping)[line];
        } else {
            (*mapping)[line] = lastMapped;
        }
    }

    int nextMapped = lastMapped;
    for (int line = lineCount; line >= 1; --line) {
        if ((*mapping)[line] > 0) {
            nextMapped = (*mapping)[line];
        } else {
            (*mapping)[line] = nextMapped;
        }
    }
}

void fillNewToOldGaps(QVector<int> *mapping, int lineCount, int oldLineCount)
{
    int lastMapped = 1;
    for (int line = 1; line <= lineCount; ++line) {
        if ((*mapping)[line] > 0) {
            lastMapped = (*mapping)[line];
        } else {
            (*mapping)[line] = lastMapped;
        }
    }

    int nextMapped = qMax(1, oldLineCount);
    for (int line = lineCount; line >= 1; --line) {
        if ((*mapping)[line] > 0) {
            nextMapped = (*mapping)[line];
        } else {
            (*mapping)[line] = nextMapped;
        }
    }
}

} // namespace

FileLineAlignment buildFileLineAlignment(const QString &before, const QString &after)
{
    const QStringList oldLines = splitSourceLines(before);
    const QStringList newLines = splitSourceLines(after);
    const int oldCount = oldLines.size();
    const int newCount = newLines.size();

    FileLineAlignment alignment;
    alignment.oldLineCount = oldCount;
    alignment.newLineCount = newCount;
    alignment.oldToNew = QVector<int>(oldCount + 1, 0);
    alignment.newToOld = QVector<int>(newCount + 1, 0);

    if (oldCount == 0 || newCount == 0) {
        return alignment;
    }

    constexpr int kMaxCells = 4'000'000;
    if (static_cast<qint64>(oldCount + 1) * (newCount + 1) > kMaxCells) {
        const int shared = qMin(oldCount, newCount);
        for (int line = 1; line <= shared; ++line) {
            alignment.oldToNew[line] = line;
            alignment.newToOld[line] = line;
        }
        fillOldToNewGaps(&alignment.oldToNew, oldCount);
        fillNewToOldGaps(&alignment.newToOld, newCount, oldCount);
        return alignment;
    }

    QVector<QVector<int>> dp(oldCount + 1, QVector<int>(newCount + 1, 0));
    for (int oldIndex = 1; oldIndex <= oldCount; ++oldIndex) {
        for (int newIndex = 1; newIndex <= newCount; ++newIndex) {
            if (oldLines.at(oldIndex - 1) == newLines.at(newIndex - 1)) {
                dp[oldIndex][newIndex] = dp[oldIndex - 1][newIndex - 1] + 1;
            } else {
                dp[oldIndex][newIndex] =
                    qMax(dp[oldIndex - 1][newIndex], dp[oldIndex][newIndex - 1]);
            }
        }
    }

    int oldIndex = oldCount;
    int newIndex = newCount;
    while (oldIndex > 0 && newIndex > 0) {
        if (oldLines.at(oldIndex - 1) == newLines.at(newIndex - 1)) {
            alignment.oldToNew[oldIndex] = newIndex;
            alignment.newToOld[newIndex] = oldIndex;
            --oldIndex;
            --newIndex;
        } else if (dp[oldIndex - 1][newIndex] >= dp[oldIndex][newIndex - 1]) {
            --oldIndex;
        } else {
            --newIndex;
        }
    }

    fillOldToNewGaps(&alignment.oldToNew, oldCount);
    fillNewToOldGaps(&alignment.newToOld, newCount, oldCount);
    return alignment;
}

void augmentFileLineAlignmentFromDiff(FileLineAlignment *alignment,
                                      const QVector<DiffLineMap> &diffMap)
{
    if (!alignment || alignment->oldLineCount == 0 || alignment->newLineCount == 0) {
        return;
    }

    for (const DiffLineMap &entry : diffMap) {
        if (entry.isHunkHeader || entry.oldLine < 1 || entry.newLine < 1) {
            continue;
        }
        if (entry.oldLine <= alignment->oldLineCount && entry.newLine <= alignment->newLineCount) {
            alignment->oldToNew[entry.oldLine] = entry.newLine;
            alignment->newToOld[entry.newLine] = entry.oldLine;
        }
    }

    fillOldToNewGaps(&alignment->oldToNew, alignment->oldLineCount);
    fillNewToOldGaps(&alignment->newToOld, alignment->newLineCount, alignment->oldLineCount);
}

void rebuildDiffLineNavigation(AlignedSideBySideView *view, const QVector<DiffLineMap> &diffMap)
{
    if (!view) {
        return;
    }

    view->diffLineToDisplayRow = QVector<int>(diffMap.size(), 0);
    for (int index = 0; index < diffMap.size(); ++index) {
        const DiffLineMap &entry = diffMap.at(index);
        int row = 0;

        if (entry.isHunkHeader) {
            if (entry.oldLine > 0) {
                row = view->beforeSourceToDisplay.value(entry.oldLine, 0);
            }
            if (row < 1 && entry.newLine > 0) {
                row = view->afterSourceToDisplay.value(entry.newLine, 0);
            }
            view->diffLineToDisplayRow[index] = row;
            continue;
        }

        if (entry.oldLine > 0 && entry.newLine > 0) {
            const int oldRow = view->beforeSourceToDisplay.value(entry.oldLine, 0);
            const int newRow = view->afterSourceToDisplay.value(entry.newLine, 0);
            row = oldRow > 0 ? oldRow : newRow;
        } else if (entry.oldLine > 0) {
            row = view->beforeSourceToDisplay.value(entry.oldLine, 0);
        } else if (entry.newLine > 0) {
            row = view->afterSourceToDisplay.value(entry.newLine, 0);
        }

        view->diffLineToDisplayRow[index] = row;
    }
}

AlignedSideBySideView buildAlignedSideBySideView(const QString &before, const QString &after,
                                                const QVector<DiffLineMap> &diffMap)
{
    const QStringList oldLines = splitSourceLines(before);
    const QStringList newLines = splitSourceLines(after);
    const int oldCount = oldLines.size();
    const int newCount = newLines.size();

    AlignedSideBySideView view;
    view.diffLineToDisplayRow = QVector<int>(diffMap.size(), 0);

    if (oldCount == 0 && newCount == 0) {
        return view;
    }

    QStringList beforeDisplay;
    QStringList afterDisplay;
    int displayRow = 0;
    int oldPtr = 1;
    int newPtr = 1;

    const auto appendMatch = [&](int oldLine, int newLine) -> bool {
        if (oldLine < 1 || newLine < 1 || oldLine > oldCount || newLine > newCount) {
            return false;
        }
        if (view.beforeSourceToDisplay.contains(oldLine)
            || view.afterSourceToDisplay.contains(newLine)) {
            return false;
        }
        ++displayRow;
        beforeDisplay.append(oldLines.at(oldLine - 1));
        afterDisplay.append(newLines.at(newLine - 1));
        view.beforeSourceToDisplay.insert(oldLine, displayRow);
        view.afterSourceToDisplay.insert(newLine, displayRow);
        return true;
    };

    const auto appendOldOnly = [&](int oldLine) -> bool {
        if (oldLine < 1 || oldLine > oldCount || view.beforeSourceToDisplay.contains(oldLine)) {
            return false;
        }
        ++displayRow;
        beforeDisplay.append(oldLines.at(oldLine - 1));
        afterDisplay.append(QString());
        view.beforeSourceToDisplay.insert(oldLine, displayRow);
        view.afterPaddingRows.insert(displayRow);
        return true;
    };

    const auto appendNewOnly = [&](int newLine) -> bool {
        if (newLine < 1 || newLine > newCount || view.afterSourceToDisplay.contains(newLine)) {
            return false;
        }
        ++displayRow;
        beforeDisplay.append(QString());
        afterDisplay.append(newLines.at(newLine - 1));
        view.afterSourceToDisplay.insert(newLine, displayRow);
        view.beforePaddingRows.insert(displayRow);
        return true;
    };

    const auto syncTo = [&](int targetOld, int targetNew) {
        while (oldPtr < targetOld && newPtr < targetNew) {
            if (!appendMatch(oldPtr, newPtr)) {
                break;
            }
            ++oldPtr;
            ++newPtr;
        }
        while (oldPtr < targetOld) {
            if (!appendOldOnly(oldPtr)) {
                break;
            }
            ++oldPtr;
        }
        while (newPtr < targetNew) {
            if (!appendNewOnly(newPtr)) {
                break;
            }
            ++newPtr;
        }
    };

    if (diffMap.isEmpty()) {
        const int shared = qMin(oldCount, newCount);
        for (int index = 0; index < shared; ++index) {
            appendMatch(index + 1, index + 1);
        }
        for (int index = shared; index < oldCount; ++index) {
            appendOldOnly(index + 1);
        }
        for (int index = shared; index < newCount; ++index) {
            appendNewOnly(index + 1);
        }
        view.beforeText = beforeDisplay.join(QLatin1Char('\n'));
        view.afterText = afterDisplay.join(QLatin1Char('\n'));
        rebuildDiffLineNavigation(&view, diffMap);
        return view;
    }

    for (int diffIndex = 0; diffIndex < diffMap.size(); ++diffIndex) {
        const DiffLineMap &entry = diffMap.at(diffIndex);

        if (entry.isHunkHeader) {
            syncTo(entry.oldLine, entry.newLine);
            continue;
        }

        const bool hasOld = entry.oldLine > 0;
        const bool hasNew = entry.newLine > 0;

        if (hasOld && hasNew) {
            syncTo(entry.oldLine, entry.newLine);
            if (appendMatch(entry.oldLine, entry.newLine)) {
                oldPtr = entry.oldLine + 1;
                newPtr = entry.newLine + 1;
            }
        } else if (hasOld) {
            syncTo(entry.oldLine, newPtr);
            if (appendOldOnly(entry.oldLine)) {
                oldPtr = entry.oldLine + 1;
                view.beforeChangedRows.insert(displayRow);
            }
        } else if (hasNew) {
            syncTo(oldPtr, entry.newLine);
            if (appendNewOnly(entry.newLine)) {
                newPtr = entry.newLine + 1;
                view.afterChangedRows.insert(displayRow);
            }
        }
    }

    syncTo(oldCount + 1, newCount + 1);

    view.beforeText = beforeDisplay.join(QLatin1Char('\n'));
    view.afterText = afterDisplay.join(QLatin1Char('\n'));
    rebuildDiffLineNavigation(&view, diffMap);
    return view;
}

void applyDiffHighlightsToAlignedView(AlignedSideBySideView *view,
                                      const QVector<DiffLineMap> &diffMap)
{
    if (!view) {
        return;
    }

    Q_UNUSED(diffMap);
    // Changed rows are assigned during buildAlignedSideBySideView.
}

} // namespace DiffParser
