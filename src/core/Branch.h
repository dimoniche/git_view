#pragma once

#include <QString>

struct Branch {
    QString name;
    QString tipHash;
    bool isRemote = false;
    bool isCurrent = false;
};
