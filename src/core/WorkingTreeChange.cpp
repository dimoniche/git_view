#include "core/WorkingTreeChange.h"

#include <QStringList>

namespace {

QString visualPorcelainChar(QChar status)
{
    if (status == QLatin1Char(' ')) {
        return QStringLiteral("·");
    }
    return QString(status);
}

} // namespace

QString WorkingTreeChange::statusLabel() const
{
    if (isUntracked()) {
        return QStringLiteral("??");
    }
    return QString(indexStatus) + workTreeStatus;
}

QString WorkingTreeChange::porcelainStatusDisplay() const
{
    if (isUntracked()) {
        return QStringLiteral("??");
    }
    return visualPorcelainChar(indexStatus) + visualPorcelainChar(workTreeStatus);
}

QString WorkingTreeChange::statusDescription() const
{
    if (isUntracked()) {
        return QStringLiteral("untracked");
    }

    QStringList parts;
    if (hasStaged()) {
        parts.append(QStringLiteral("staged"));
    }
    if (hasUnstaged()) {
        parts.append(QStringLiteral("unstaged"));
    }
    if (parts.isEmpty()) {
        parts.append(QStringLiteral("changed"));
    }
    return parts.join(QStringLiteral(" + "));
}
