#pragma once

#include <QString>

struct Branch {
    QString name;
    QString tipHash;
    QString upstream;
    bool isRemote = false;
    bool isCurrent = false;

    bool hasUpstream() const { return !upstream.isEmpty(); }
};
