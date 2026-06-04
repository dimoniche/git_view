#pragma once

#include "core/Commit.h"

#include <vector>

struct GraphEdge {
    int fromRow = 0;
    int fromLane = 0;
    int toRow = 0;
    int toLane = 0;
};

struct GraphLayout {
    std::vector<int> lanes;
    int laneCount = 0;
    std::vector<GraphEdge> edges;

    static GraphLayout build(const std::vector<Commit> &commits);
};
