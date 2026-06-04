#pragma once

#include <QString>

namespace DiffParser {

QString extractFilePatch(const QString &fullPatch, const QString &path);
bool diffHeaderMatchesPath(const QString &diffHeaderLine, const QString &path);

} // namespace DiffParser
