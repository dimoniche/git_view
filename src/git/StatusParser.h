#pragma once

#include "core/WorkingTreeChange.h"

#include <QString>

namespace StatusParser {

QString normalizeGitPath(QString path);
QString pathFromPorcelainLine(const QString &line);
bool parsePorcelainLine(const QString &line, WorkingTreeChange *change);

} // namespace StatusParser
