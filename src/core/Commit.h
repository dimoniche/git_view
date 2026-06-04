#pragma once

#include <QString>
#include <QStringList>

struct Commit {
    QString hash;
    QStringList parentHashes;
    QString author;
    QString date;
    QString subject;

    bool isMerge() const { return parentHashes.size() > 1; }
};
