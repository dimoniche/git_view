#pragma once

#include "core/Commit.h"

#include <QString>
#include <vector>

class LogParser {
public:
    static std::vector<Commit> parseLogOutput(const QString &output);
};
