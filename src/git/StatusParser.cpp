#include "git/StatusParser.h"

namespace StatusParser {

QString normalizeGitPath(QString path)
{
    path = path.trimmed();
    if (path.size() >= 2 && path.front() == QLatin1Char('"') && path.back() == QLatin1Char('"')) {
        path = path.mid(1, path.size() - 2);
        path.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
        path.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    }
    return path;
}

QString pathFromPorcelainLine(const QString &line)
{
    if (line.size() < 2) {
        return {};
    }

    int index = 2;
    while (index < line.size() && line.at(index).isSpace()) {
        ++index;
    }
    return normalizeGitPath(line.mid(index));
}

bool parsePorcelainLine(const QString &line, WorkingTreeChange *change)
{
    if (!change || line.size() < 2) {
        return false;
    }

    change->indexStatus = line.at(0);
    change->workTreeStatus = line.at(1);
    change->path = pathFromPorcelainLine(line);
    return !change->path.isEmpty();
}

} // namespace StatusParser
