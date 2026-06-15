#pragma once

#include <QString>

namespace PathUtils {

QString normalizeExternalPath(const QString &raw);
QString toRepoRelativePath(const QString &repoRoot, const QString &path);

} // namespace PathUtils
