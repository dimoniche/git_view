#include "git/StatusParser.h"

namespace StatusParser {

QString unescapeGitQuotedPath(QString path)
{
    QByteArray bytes;
    bytes.reserve(path.size());

    auto appendUtf8 = [&](const QByteArray &chunk) {
        bytes.append(chunk);
    };

    for (int i = 0; i < path.size(); ++i) {
        const QChar c = path.at(i);
        if (c != QLatin1Char('\\')) {
            appendUtf8(QString(c).toUtf8());
            continue;
        }
        if (i + 1 >= path.size()) {
            appendUtf8("\\");
            continue;
        }

        const QChar next = path.at(i + 1);
        if (next == QLatin1Char('n')) {
            appendUtf8("\n");
            i += 1;
            continue;
        }
        if (next == QLatin1Char('t')) {
            appendUtf8("\t");
            i += 1;
            continue;
        }
        if (next == QLatin1Char('"')) {
            appendUtf8("\"");
            i += 1;
            continue;
        }
        if (next == QLatin1Char('\\')) {
            appendUtf8("\\");
            i += 1;
            continue;
        }

        if (i + 3 < path.size()) {
            bool isOctal = true;
            for (int j = 1; j <= 3; ++j) {
                const QChar digit = path.at(i + j);
                if (digit < QLatin1Char('0') || digit > QLatin1Char('7')) {
                    isOctal = false;
                    break;
                }
            }
            if (isOctal) {
                const QString oct = path.mid(i + 1, 3);
                bytes.append(static_cast<char>(oct.toUInt(nullptr, 8)));
                i += 3;
                continue;
            }
        }

        appendUtf8("\\");
    }

    return QString::fromUtf8(bytes);
}

QString normalizeGitPath(QString path)
{
    path.remove(QLatin1Char('\r'));
    path = path.trimmed();
    const bool wasQuoted =
        path.size() >= 2 && path.front() == QLatin1Char('"') && path.back() == QLatin1Char('"');
    if (wasQuoted) {
        path = path.mid(1, path.size() - 2);
        if (path.contains(QLatin1Char('\\'))) {
            path = unescapeGitQuotedPath(path);
        } else {
            path.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
            path.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        }
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
