#include "git/PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace PathUtils {

QString normalizeExternalPath(const QString &raw)
{
    QString path = raw.trimmed();
    if (path.isEmpty()) {
        return path;
    }

    if (path.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
        const QUrl url(path);
        if (url.isValid() && url.isLocalFile()) {
            return QDir::fromNativeSeparators(url.toLocalFile());
        }
        QString remainder = path.mid(5);
        if (remainder.startsWith(QLatin1String("//"))) {
            remainder = remainder.mid(2);
        } else if (remainder.startsWith(QLatin1Char('/'))) {
            remainder = remainder.mid(1);
        }
        if (!remainder.isEmpty()) {
            return QDir::fromNativeSeparators(QUrl::fromPercentEncoding(remainder.toUtf8()));
        }
    }

    if (path.startsWith(QLatin1String(":/"))) {
        const QString absolute = path.mid(1);
        if (absolute.startsWith(QLatin1Char('/'))) {
            return absolute;
        }
    }

    return path;
}

QString toRepoRelativePath(const QString &repoRoot, const QString &path)
{
    const QString normalized = normalizeExternalPath(path);
    if (normalized.isEmpty() || repoRoot.isEmpty()) {
        return {};
    }

    const QFileInfo info(normalized);
    if (info.isAbsolute()) {
        QDir repoDir(repoRoot);
        const QString relative = repoDir.relativeFilePath(info.absoluteFilePath());
        if (!relative.isEmpty() && !relative.startsWith(QLatin1String(".."))) {
            return relative;
        }
        return {};
    }

    return normalized;
}

} // namespace PathUtils
