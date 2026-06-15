#pragma once

#include <QString>

struct WorkingTreeChange {
    QString path;
    QChar indexStatus = QLatin1Char(' ');
    QChar workTreeStatus = QLatin1Char(' ');

    bool isUntracked() const
    {
        return indexStatus == QLatin1Char('?') && workTreeStatus == QLatin1Char('?');
    }

    bool hasStaged() const
    {
        return indexStatus != QLatin1Char(' ') && indexStatus != QLatin1Char('?');
    }

    bool hasUnstaged() const { return workTreeStatus != QLatin1Char(' ') || isUntracked(); }

    QString statusLabel() const;
    QString statusDescription() const;
    QString porcelainStatusDisplay() const;
};
