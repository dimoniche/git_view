#include "core/GraphLayout.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <algorithm>

namespace {

constexpr int kTrunkMarker = 100000;

QHash<QString, int> buildRowOf(const std::vector<Commit> &commits)
{
    QHash<QString, int> rowOf;
    rowOf.reserve(static_cast<int>(commits.size()));
    for (size_t i = 0; i < commits.size(); ++i) {
        rowOf.insert(commits[i].hash, static_cast<int>(i));
    }
    return rowOf;
}

QSet<QString> trunkCommits(const std::vector<Commit> &commits,
                           const QHash<QString, int> &rowOf,
                           const QString &tipHash)
{
    QSet<QString> onTrunk;
    QString current = tipHash;
    while (!current.isEmpty()) {
        if (onTrunk.contains(current)) {
            break;
        }
        onTrunk.insert(current);

        const int row = rowOf.value(current, -1);
        if (row < 0) {
            break;
        }
        const Commit &commit = commits[static_cast<size_t>(row)];
        if (commit.parentHashes.isEmpty()) {
            break;
        }
        current = commit.parentHashes.front();
    }
    return onTrunk;
}

void assignSideBranch(const std::vector<Commit> &commits,
                      const QHash<QString, int> &rowOf,
                      const QSet<QString> &onTrunk,
                      const QString &startHash,
                      int sideLane,
                      std::vector<int> &lanes)
{
    QString current = startHash;
    while (!current.isEmpty()) {
        if (onTrunk.contains(current)) {
            break;
        }

        const int row = rowOf.value(current, -1);
        if (row < 0) {
            break;
        }

        lanes[static_cast<size_t>(row)] = sideLane;

        const Commit &commit = commits[static_cast<size_t>(row)];
        if (commit.parentHashes.isEmpty()) {
            break;
        }
        current = commit.parentHashes.front();
    }
}

int allocSideLane(int mergeRow, const std::vector<int> &lanes)
{
    QSet<int> usedBelow;
    for (size_t row = static_cast<size_t>(mergeRow + 1); row < lanes.size(); ++row) {
        const int lane = lanes[row];
        if (lane >= 0 && lane != kTrunkMarker) {
            usedBelow.insert(lane);
        }
    }

    int lane = 1;
    while (usedBelow.contains(lane)) {
        ++lane;
    }
    return lane;
}

void finalizeTrunkLane(std::vector<int> &lanes, std::vector<GraphEdge> &edges)
{
    constexpr int trunkLane = 0;
    for (int &lane : lanes) {
        if (lane == kTrunkMarker) {
            lane = trunkLane;
        }
    }
    for (GraphEdge &edge : edges) {
        if (edge.fromLane == kTrunkMarker) {
            edge.fromLane = trunkLane;
        }
        if (edge.toLane == kTrunkMarker) {
            edge.toLane = trunkLane;
        }
    }
}

QString shortBranchName(const QString &name)
{
    const int slash = name.indexOf(QLatin1Char('/'));
    if (slash > 0 && (name.startsWith(QStringLiteral("origin/"))
                      || name.startsWith(QStringLiteral("refs/heads/")))) {
        return name.mid(slash + 1);
    }
    return name;
}

QString branchNameForHash(const QString &hash, const std::vector<Branch> &branches)
{
    QString best;
    for (const Branch &branch : branches) {
        if (branch.tipHash != hash) {
            continue;
        }
        const QString candidate = shortBranchName(branch.name);
        if (best.isEmpty() || (!branch.isRemote && best.contains(QLatin1Char('/')))) {
            best = candidate;
        }
    }
    return best;
}

QString branchNameFromMergeSubject(const QString &subject)
{
    static const QRegularExpression patterns[] = {
        QRegularExpression(QStringLiteral("Merge branch '([^']+)'")),
        QRegularExpression(QStringLiteral("Merge remote-tracking branch '([^']+)'")),
        QRegularExpression(
            QStringLiteral("Merge pull request .* from ([^\\s]+)")),
    };

    for (const QRegularExpression &pattern : patterns) {
        const QRegularExpressionMatch match = pattern.match(subject);
        if (match.hasMatch()) {
            QString name = match.captured(1).trimmed();
            const int slash = name.indexOf(QLatin1Char('/'));
            if (slash > 0) {
                name = name.mid(slash + 1);
            }
            if (!name.isEmpty()) {
                return name;
            }
        }
    }
    return {};
}

QString resolveSideBranchLabel(const Commit &mergeCommit,
                               const QString &mergedParentHash,
                               const std::vector<Branch> &branches)
{
    QString name = branchNameFromMergeSubject(mergeCommit.subject);
    if (!name.isEmpty()) {
        return name;
    }

    name = branchNameForHash(mergedParentHash, branches);
    if (!name.isEmpty()) {
        return name;
    }

    return mergedParentHash.left(8);
}

int maxAssignedLane(const std::vector<int> &lanes)
{
    int maxLane = 0;
    for (int lane : lanes) {
        if (lane >= 0 && lane != kTrunkMarker) {
            maxLane = std::max(maxLane, lane);
        }
    }
    return maxLane;
}

GraphLayout buildForBranch(const std::vector<Commit> &commits,
                           const QString &branchTipHash,
                           const std::vector<Branch> &branches)
{
    GraphLayout layout;
    const QHash<QString, int> rowOf = buildRowOf(commits);
    const QSet<QString> onTrunk = trunkCommits(commits, rowOf, branchTipHash);

    layout.lanes.assign(commits.size(), -1);

    for (size_t row = 0; row < commits.size(); ++row) {
        const Commit &commit = commits[row];
        if (onTrunk.contains(commit.hash)) {
            layout.lanes[row] = kTrunkMarker;
        }

        for (int parentIndex = 0; parentIndex < commit.parentHashes.size(); ++parentIndex) {
            const QString &parentHash = commit.parentHashes[parentIndex];
            const int parentRow = rowOf.value(parentHash, -1);
            if (parentRow < 0) {
                continue;
            }

            if (onTrunk.contains(commit.hash)) {
                const int fromLane = kTrunkMarker;
                int toLane = kTrunkMarker;

                if (parentIndex == 0) {
                    toLane = kTrunkMarker;
                } else {
                    toLane = allocSideLane(static_cast<int>(row), layout.lanes);
                    assignSideBranch(commits, rowOf, onTrunk, parentHash, toLane, layout.lanes);

                    GraphLaneLabel label;
                    label.lane = toLane;
                    label.row = static_cast<int>(row);
                    label.name = resolveSideBranchLabel(commit, parentHash, branches);
                    layout.laneLabels.push_back(std::move(label));
                }

                GraphEdge edge;
                edge.fromRow = static_cast<int>(row);
                edge.fromLane = fromLane;
                edge.toRow = parentRow;
                edge.toLane = toLane;
                layout.edges.push_back(edge);
                continue;
            }

            int lane = layout.lanes[row];
            if (lane < 0) {
                lane = allocSideLane(static_cast<int>(row) - 1, layout.lanes);
                layout.lanes[row] = lane;
            }

            int toLane = lane;
            if (onTrunk.contains(parentHash)) {
                toLane = kTrunkMarker;
            } else if (layout.lanes[static_cast<size_t>(parentRow)] >= 0) {
                toLane = layout.lanes[static_cast<size_t>(parentRow)];
            }

            GraphEdge edge;
            edge.fromRow = static_cast<int>(row);
            edge.fromLane = lane;
            edge.toRow = parentRow;
            edge.toLane = toLane;
            layout.edges.push_back(edge);
        }
    }

    for (size_t row = 0; row < commits.size(); ++row) {
        if (layout.lanes[row] < 0) {
            layout.lanes[row] = allocSideLane(static_cast<int>(row) - 1, layout.lanes);
        }
    }

    finalizeTrunkLane(layout.lanes, layout.edges);
    layout.laneCount = std::max(maxAssignedLane(layout.lanes) + 1, 1);
    return layout;
}

GraphLayout buildGeneral(const std::vector<Commit> &commits)
{
    GraphLayout layout;
    const QHash<QString, int> rowOf = buildRowOf(commits);

    layout.lanes.assign(commits.size(), -1);
    QHash<QString, int> reservedLane;

    QVector<int> freeLanes;
    int nextLane = 0;

    const auto allocLane = [&]() {
        if (!freeLanes.isEmpty()) {
            return freeLanes.takeLast();
        }
        return nextLane++;
    };

    const auto releaseLane = [&](int lane) {
        if (lane >= 0) {
            freeLanes.push_back(lane);
        }
    };

    for (size_t row = 0; row < commits.size(); ++row) {
        const Commit &commit = commits[row];

        int lane = reservedLane.value(commit.hash, -1);
        if (lane < 0) {
            lane = allocLane();
        }
        layout.lanes[row] = lane;
        reservedLane.remove(commit.hash);

        const int parentCount = commit.parentHashes.size();
        for (int parentIndex = 0; parentIndex < parentCount; ++parentIndex) {
            const QString &parentHash = commit.parentHashes[parentIndex];
            const int parentRow = rowOf.value(parentHash, -1);
            if (parentRow < 0) {
                continue;
            }

            const int parentLane = (parentIndex == 0) ? lane : allocLane();
            GraphEdge edge;
            edge.fromRow = static_cast<int>(row);
            edge.fromLane = lane;
            edge.toRow = parentRow;
            edge.toLane = parentLane;
            layout.edges.push_back(edge);

            reservedLane.insert(parentHash, parentLane);
            if (layout.lanes[static_cast<size_t>(parentRow)] < 0) {
                layout.lanes[static_cast<size_t>(parentRow)] = parentLane;
            }
        }

        if (parentCount == 0) {
            releaseLane(lane);
        }
    }

    for (size_t row = 0; row < commits.size(); ++row) {
        if (layout.lanes[row] < 0) {
            layout.lanes[row] = allocLane();
        }
    }
    layout.laneCount = std::max(nextLane, maxAssignedLane(layout.lanes) + 1);

    return layout;
}

} // namespace

GraphLayout GraphLayout::build(const std::vector<Commit> &commits,
                               const QString &branchTipHash,
                               const std::vector<Branch> &branches)
{
    if (commits.empty()) {
        return {};
    }

    if (!branchTipHash.trimmed().isEmpty()) {
        return buildForBranch(commits, branchTipHash.trimmed(), branches);
    }

    return buildGeneral(commits);
}
