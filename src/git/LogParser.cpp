#include "git/LogParser.h"

#include <QStringView>

namespace {

constexpr int kHash = 0;
constexpr int kParents = 1;
constexpr int kAuthor = 2;
constexpr int kDate = 3;
constexpr int kSubject = 4;
constexpr int kFieldCount = 5;

QStringList splitParents(const QString &parentsField)
{
    if (parentsField.isEmpty()) {
        return {};
    }
    const QStringList parts = parentsField.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return parts;
}

} // namespace

std::vector<Commit> LogParser::parseLogOutput(const QString &output)
{
    std::vector<Commit> commits;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    commits.reserve(static_cast<size_t>(lines.size()));

    for (const QString &line : lines) {
        const QList<QStringView> fields =
            QStringView(line).split(QLatin1Char('\t'), Qt::KeepEmptyParts);
        if (fields.size() < kFieldCount) {
            continue;
        }

        Commit commit;
        commit.hash = fields[kHash].toString();
        commit.parentHashes = splitParents(fields[kParents].toString());
        commit.author = fields[kAuthor].toString();
        commit.date = fields[kDate].toString();
        commit.subject = fields[kSubject].toString();
        commits.push_back(std::move(commit));
    }

    return commits;
}
