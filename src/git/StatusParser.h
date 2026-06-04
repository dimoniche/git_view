#pragma once

#include "core/WorkingTreeChange.h"

#include <QString>

namespace StatusParser {

QString normalizeGitPath(QString path);
QString normalizePorcelainLine(QString line);
QString pathFromPorcelainLine(const QString &line);
bool parsePorcelainLine(const QString &line, WorkingTreeChange *change);

} // namespace StatusParser
