#include "core/GraphLayout.h"

#include <QHash>
#include <algorithm>

GraphLayout GraphLayout::build(const std::vector<Commit> &commits)
{
    GraphLayout layout;
    if (commits.empty()) {
        return layout;
    }

    QHash<QString, int> rowOf;
    rowOf.reserve(static_cast<int>(commits.size()));
    for (size_t i = 0; i < commits.size(); ++i) {
        rowOf.insert(commits[i].hash, static_cast<int>(i));
    }

    layout.lanes.assign(commits.size(), -1);
    QHash<QString, int> reservedLane;

    int nextLane = 0;

    for (size_t row = 0; row < commits.size(); ++row) {
        const Commit &commit = commits[row];

        int lane = reservedLane.value(commit.hash, -1);
        if (lane < 0) {
            lane = nextLane++;
        }
        layout.lanes[row] = lane;
        reservedLane.remove(commit.hash);

        for (int parentIndex = 0; parentIndex < commit.parentHashes.size(); ++parentIndex) {
            const QString &parentHash = commit.parentHashes[parentIndex];
            const int parentRow = rowOf.value(parentHash, -1);
            if (parentRow < 0) {
                continue;
            }

            const int parentLane = (parentIndex == 0) ? lane : nextLane++;
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

        layout.laneCount = std::max(layout.laneCount, nextLane);
    }

    for (size_t row = 0; row < commits.size(); ++row) {
        if (layout.lanes[row] < 0) {
            layout.lanes[row] = nextLane++;
        }
    }
    layout.laneCount = std::max(layout.laneCount, nextLane);

    return layout;
}
