#include "git/DiffParser.h"

#include <QStringList>

namespace DiffParser {

bool diffHeaderMatchesPath(const QString &diffHeaderLine, const QString &path)
{
    if (!diffHeaderLine.startsWith(QStringLiteral("diff --git ")) || path.isEmpty()) {
        return false;
    }

    const QString quoted = QStringLiteral("/") + path;
    return diffHeaderLine.contains(QStringLiteral(" a") + quoted)
           || diffHeaderLine.contains(QStringLiteral(" b") + quoted)
           || diffHeaderLine.contains(QStringLiteral(" a/") + path)
           || diffHeaderLine.contains(QStringLiteral(" b/") + path);
}

QString extractFilePatch(const QString &fullPatch, const QString &path)
{
    if (fullPatch.isEmpty() || path.isEmpty()) {
        return {};
    }

    const QStringList lines = fullPatch.split(QLatin1Char('\n'));
    QStringList chunk;
    bool capturing = false;

    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("diff --git "))) {
            if (capturing) {
                break;
            }
            capturing = diffHeaderMatchesPath(line, path);
            if (capturing) {
                chunk.clear();
                chunk.append(line);
            }
            continue;
        }
        if (capturing) {
            chunk.append(line);
        }
    }

    return chunk.join(QLatin1Char('\n'));
}

} // namespace DiffParser
