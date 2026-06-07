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

    for (const QString &line : lines) {
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

AlignedSideBySideView buildAlignedSideBySideView(const QString &before, const QString &after)
{
    const QStringList oldLines = splitSourceLines(before);
    const QStringList newLines = splitSourceLines(after);
    const int oldCount = oldLines.size();
    const int newCount = newLines.size();

    AlignedSideBySideView view;
    if (oldCount == 0 && newCount == 0) {
        return view;
    }

    if (oldCount == 0) {
        QStringList afterDisplay;
        for (int index = 0; index < newCount; ++index) {
            afterDisplay.append(newLines.at(index));
            view.afterSourceToDisplay.insert(index + 1, index + 1);
            view.beforePaddingRows.insert(index + 1);
        }
        view.beforeText = QString(newCount, QLatin1Char('\n'));
        view.afterText = afterDisplay.join(QLatin1Char('\n'));
        return view;
    }

    if (newCount == 0) {
        QStringList beforeDisplay;
        for (int index = 0; index < oldCount; ++index) {
            beforeDisplay.append(oldLines.at(index));
            view.beforeSourceToDisplay.insert(index + 1, index + 1);
            view.afterPaddingRows.insert(index + 1);
        }
        view.beforeText = beforeDisplay.join(QLatin1Char('\n'));
        view.afterText = QString(oldCount, QLatin1Char('\n'));
        return view;
    }

    constexpr int kMaxCells = 4'000'000;
    if (static_cast<qint64>(oldCount + 1) * (newCount + 1) > kMaxCells) {
        const int shared = qMin(oldCount, newCount);
        QStringList beforeDisplay;
        QStringList afterDisplay;
        for (int index = 0; index < shared; ++index) {
            beforeDisplay.append(oldLines.at(index));
            afterDisplay.append(newLines.at(index));
            const int displayRow = index + 1;
            view.beforeSourceToDisplay.insert(index + 1, displayRow);
            view.afterSourceToDisplay.insert(index + 1, displayRow);
        }
        for (int index = shared; index < oldCount; ++index) {
            beforeDisplay.append(oldLines.at(index));
            afterDisplay.append(QString());
            const int displayRow = beforeDisplay.size();
            view.beforeSourceToDisplay.insert(index + 1, displayRow);
            view.afterPaddingRows.insert(displayRow);
        }
        for (int index = shared; index < newCount; ++index) {
            beforeDisplay.append(QString());
            afterDisplay.append(newLines.at(index));
            const int displayRow = beforeDisplay.size();
            view.afterSourceToDisplay.insert(index + 1, displayRow);
            view.beforePaddingRows.insert(displayRow);
        }
        view.beforeText = beforeDisplay.join(QLatin1Char('\n'));
        view.afterText = afterDisplay.join(QLatin1Char('\n'));
        return view;
    }

    QVector<QVector<int>> dp(oldCount + 1, QVector<int>(newCount + 1, 0));
    for (int oldIdx = 1; oldIdx <= oldCount; ++oldIdx) {
        for (int newIdx = 1; newIdx <= newCount; ++newIdx) {
            if (oldLines.at(oldIdx - 1) == newLines.at(newIdx - 1)) {
                dp[oldIdx][newIdx] = dp[oldIdx - 1][newIdx - 1] + 1;
            } else {
                dp[oldIdx][newIdx] = qMax(dp[oldIdx - 1][newIdx], dp[oldIdx][newIdx - 1]);
            }
        }
    }

    enum class EditKind { Match, OldOnly, NewOnly };
    struct Edit {
        EditKind kind;
        int oldIndex = 0;
        int newIndex = 0;
    };

    QVector<Edit> edits;
    int oldIdx = oldCount;
    int newIdx = newCount;
    while (oldIdx > 0 || newIdx > 0) {
        if (oldIdx > 0 && newIdx > 0 && oldLines.at(oldIdx - 1) == newLines.at(newIdx - 1)) {
            edits.prepend({EditKind::Match, oldIdx, newIdx});
            --oldIdx;
            --newIdx;
        } else if (newIdx > 0
                   && (oldIdx == 0 || dp[oldIdx][newIdx - 1] >= dp[oldIdx - 1][newIdx])) {
            edits.prepend({EditKind::NewOnly, 0, newIdx});
            --newIdx;
        } else {
            edits.prepend({EditKind::OldOnly, oldIdx, 0});
            --oldIdx;
        }
    }

    QStringList beforeDisplay;
    QStringList afterDisplay;
    int displayRow = 0;
    for (const Edit &edit : edits) {
        ++displayRow;
        switch (edit.kind) {
        case EditKind::Match:
            beforeDisplay.append(oldLines.at(edit.oldIndex - 1));
            afterDisplay.append(newLines.at(edit.newIndex - 1));
            view.beforeSourceToDisplay.insert(edit.oldIndex, displayRow);
            view.afterSourceToDisplay.insert(edit.newIndex, displayRow);
            break;
        case EditKind::OldOnly:
            beforeDisplay.append(oldLines.at(edit.oldIndex - 1));
            afterDisplay.append(QString());
            view.beforeSourceToDisplay.insert(edit.oldIndex, displayRow);
            view.afterPaddingRows.insert(displayRow);
            break;
        case EditKind::NewOnly:
            beforeDisplay.append(QString());
            afterDisplay.append(newLines.at(edit.newIndex - 1));
            view.afterSourceToDisplay.insert(edit.newIndex, displayRow);
            view.beforePaddingRows.insert(displayRow);
            break;
        }
    }

    view.beforeText = beforeDisplay.join(QLatin1Char('\n'));
    view.afterText = afterDisplay.join(QLatin1Char('\n'));
    return view;
}

void applyDiffHighlightsToAlignedView(AlignedSideBySideView *view,
                                      const QVector<DiffLineMap> &diffMap)
{
    if (!view) {
        return;
    }

    view->beforeChangedRows.clear();
    view->afterChangedRows.clear();

    for (const DiffLineMap &entry : diffMap) {
        if (entry.isHunkHeader) {
            continue;
        }

        const bool hasOld = entry.oldLine > 0;
        const bool hasNew = entry.newLine > 0;

        if (hasOld && !hasNew) {
            const int displayRow = view->beforeSourceToDisplay.value(entry.oldLine, 0);
            if (displayRow > 0) {
                view->beforeChangedRows.insert(displayRow);
            }
        } else if (hasNew && !hasOld) {
            const int displayRow = view->afterSourceToDisplay.value(entry.newLine, 0);
            if (displayRow > 0) {
                view->afterChangedRows.insert(displayRow);
            }
        }
    }
}

} // namespace DiffParser
