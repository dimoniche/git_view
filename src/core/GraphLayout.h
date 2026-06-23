#pragma once

#include "core/Branch.h"
#include "core/Commit.h"
#include "core/Tag.h"

#include <QString>
#include <vector>

struct GraphEdge {
    int fromRow = 0;
    int fromLane = 0;
    int toRow = 0;
    int toLane = 0;
};

struct GraphLaneLabel {
    int lane = 0;
    int row = 0;
    QString name;
};

enum class GraphRefKind {
    Branch,
    Tag,
};

struct GraphRowLabel {
    int row = 0;
    QString name;
    GraphRefKind kind = GraphRefKind::Tag;
};

struct GraphLayout {
    std::vector<int> lanes;
    int laneCount = 0;
    std::vector<GraphEdge> edges;
    std::vector<GraphLaneLabel> laneLabels;
    std::vector<GraphRowLabel> rowLabels;

    static GraphLayout build(const std::vector<Commit> &commits,
                             const QString &branchTipHash = {},
                             const std::vector<Branch> &branches = {},
                             const std::vector<Tag> &tags = {});
};
