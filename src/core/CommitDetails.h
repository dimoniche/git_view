#pragma once

#include "core/Commit.h"

#include <QString>
#include <vector>

struct CommitFileChange {
    QString status;
    QString path;
};

struct CommitDetails {
    Commit commit;
    std::vector<CommitFileChange> files;
};
