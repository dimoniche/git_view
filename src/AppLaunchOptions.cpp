#include "AppLaunchOptions.h"

#include "git/PathUtils.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>

namespace {

QString repoRelativePath(const QString &repoRoot, const QString &path)
{
    const QString relative = PathUtils::toRepoRelativePath(repoRoot, path);
    if (relative.isEmpty()) {
        return {};
    }

    const QFileInfo info(PathUtils::normalizeExternalPath(path));
    if (!info.exists() && !QFileInfo(QDir(repoRoot).filePath(relative)).exists()) {
        return {};
    }

    return relative;
}

QString discoverRepo(GitService *git, const QString &path)
{
    if (!git || path.isEmpty()) {
        return {};
    }
    return git->discoverGitDir(path);
}

void resolvePaths(AppLaunchOptions *options, GitService *git)
{
    if (!options->valid || options->showHelp) {
        return;
    }

    options->repoPath = PathUtils::normalizeExternalPath(options->repoPath);

    QStringList normalizedInput;
    normalizedInput.reserve(options->inputPaths.size());
    for (const QString &path : options->inputPaths) {
        normalizedInput.append(PathUtils::normalizeExternalPath(path));
    }
    options->inputPaths = normalizedInput;

    QStringList normalizedFiles;
    normalizedFiles.reserve(options->filePaths.size());
    for (const QString &path : options->filePaths) {
        normalizedFiles.append(PathUtils::normalizeExternalPath(path));
    }
    options->filePaths = normalizedFiles;

    if (options->repoPath.isEmpty()) {
        for (const QString &path : options->inputPaths) {
            const QString discovered = discoverRepo(git, path);
            if (!discovered.isEmpty()) {
                options->repoPath = discovered;
                break;
            }
        }
    }

    if (options->repoPath.isEmpty()) {
        for (const QString &path : options->filePaths) {
            const QString discovered = discoverRepo(git, path);
            if (!discovered.isEmpty()) {
                options->repoPath = discovered;
                break;
            }
        }
    }

    if (options->repoPath.isEmpty() && !options->inputPaths.isEmpty()) {
        options->valid = false;
        options->errorMessage = QStringLiteral("No Git repository found for the given path(s).");
        return;
    }

    if (options->repoPath.isEmpty()) {
        return;
    }

    const QDir repoDir(options->repoPath);
    for (const QString &path : options->inputPaths) {
        const QString relative = repoRelativePath(options->repoPath, path);
        if (!relative.isEmpty() && relative != QLatin1String(".")) {
            options->filePaths.append(relative);
        }
    }

    QStringList repoRelativeFiles;
    for (const QString &path : options->filePaths) {
        const QString relative = PathUtils::toRepoRelativePath(options->repoPath, path);
        if (relative.isEmpty()) {
            options->valid = false;
            options->errorMessage =
                QStringLiteral("Path is outside the repository: %1").arg(path);
            return;
        }
        repoRelativeFiles.append(relative);
    }
    options->filePaths = repoRelativeFiles;

    switch (options->action) {
    case LaunchAction::FileHistory:
    case LaunchAction::FileDiff:
        if (options->filePaths.isEmpty()) {
            options->valid = false;
            options->errorMessage =
                QStringLiteral("Action \"%1\" requires at least one file path.")
                    .arg(AppLaunchOptions::actionToString(options->action));
        }
        break;
    default:
        break;
    }
}

} // namespace

QString AppLaunchOptions::helpText()
{
    return QStringLiteral(
        "git_view — Git GUI for Linux\n"
        "\n"
        "Usage:\n"
        "  git_view [options] [path...]\n"
        "\n"
        "Options:\n"
        "  -h, --help                 Show this help and exit\n"
        "  --repo <path>              Repository root (auto-detected from paths)\n"
        "  --action <name>            Action after opening (default: open)\n"
        "  --file <path>              File path (repeatable; repo-relative or absolute)\n"
        "  --diff-scope <scope>       For file-diff: unstaged, staged, head (default: unstaged)\n"
        "\n"
        "Actions:\n"
        "  open              Open the repository (default)\n"
        "  working-changes   Show the working tree tab\n"
        "  file-history      Show commit history for the selected file(s)\n"
        "  file-diff         Show diff for the selected file(s)\n"
        "  commit            Open working tree tab to commit changes\n"
        "  log               Focus commit history\n"
        "\n"
        "Examples:\n"
        "  git_view /path/to/repo\n"
        "  git_view --repo /path/to/repo --action working-changes\n"
        "  git_view --action file-history --file src/main.cpp /path/to/repo\n"
        "  git_view --action file-diff --diff-scope staged --file README.md\n");
}

LaunchAction AppLaunchOptions::actionFromString(const QString &value, bool *ok)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("open")) {
        if (ok) {
            *ok = true;
        }
        return LaunchAction::Open;
    }
    if (normalized == QLatin1String("working-changes")
        || normalized == QLatin1String("working")) {
        if (ok) {
            *ok = true;
        }
        return LaunchAction::WorkingChanges;
    }
    if (normalized == QLatin1String("file-history")
        || normalized == QLatin1String("history")) {
        if (ok) {
            *ok = true;
        }
        return LaunchAction::FileHistory;
    }
    if (normalized == QLatin1String("file-diff") || normalized == QLatin1String("diff")) {
        if (ok) {
            *ok = true;
        }
        return LaunchAction::FileDiff;
    }
    if (normalized == QLatin1String("commit")) {
        if (ok) {
            *ok = true;
        }
        return LaunchAction::Commit;
    }
    if (normalized == QLatin1String("log")) {
        if (ok) {
            *ok = true;
        }
        return LaunchAction::Log;
    }
    if (ok) {
        *ok = false;
    }
    return LaunchAction::Open;
}

QString AppLaunchOptions::actionToString(LaunchAction action)
{
    switch (action) {
    case LaunchAction::Open:
        return QStringLiteral("open");
    case LaunchAction::WorkingChanges:
        return QStringLiteral("working-changes");
    case LaunchAction::FileHistory:
        return QStringLiteral("file-history");
    case LaunchAction::FileDiff:
        return QStringLiteral("file-diff");
    case LaunchAction::Commit:
        return QStringLiteral("commit");
    case LaunchAction::Log:
        return QStringLiteral("log");
    }
    return QStringLiteral("open");
}

AppLaunchOptions AppLaunchOptions::parse(const QStringList &arguments, GitService *git)
{
    AppLaunchOptions options;

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("git_view"));
    parser.addHelpOption();

    QCommandLineOption repoOption(QStringList{QStringLiteral("repo")},
                                  QStringLiteral("Repository root path."), QStringLiteral("path"));
    parser.addOption(repoOption);

    QCommandLineOption actionOption(QStringList{QStringLiteral("action")},
                                    QStringLiteral("Action to perform after opening."),
                                    QStringLiteral("name"), QStringLiteral("open"));
    parser.addOption(actionOption);

    QCommandLineOption fileOption(QStringList{QStringLiteral("file")},
                                  QStringLiteral("File inside the repository (repeatable)."),
                                  QStringLiteral("path"));
    parser.addOption(fileOption);

    QCommandLineOption diffScopeOption(
        QStringList{QStringLiteral("diff-scope")},
        QStringLiteral("Diff scope for file-diff: unstaged, staged, head."),
        QStringLiteral("scope"), QStringLiteral("unstaged"));
    parser.addOption(diffScopeOption);

    QStringList args = arguments;
    if (!parser.parse(args)) {
        options.valid = false;
        options.errorMessage = parser.errorText();
        return options;
    }

    if (parser.isSet(QStringLiteral("help"))) {
        options.showHelp = true;
        return options;
    }

    if (parser.isSet(repoOption)) {
        const QString repo = PathUtils::normalizeExternalPath(parser.value(repoOption));
        const QFileInfo repoInfo(repo);
        if (!repoInfo.exists()) {
            options.valid = false;
            options.errorMessage = QStringLiteral("Repository path does not exist: %1").arg(repo);
            return options;
        }
        options.repoPath = git ? discoverRepo(git, repo) : repoInfo.absoluteFilePath();
        if (options.repoPath.isEmpty()) {
            options.valid = false;
            options.errorMessage = QStringLiteral("Not a Git repository: %1").arg(repo);
            return options;
        }
    }

    bool actionOk = true;
    options.action = actionFromString(parser.value(actionOption), &actionOk);
    if (!actionOk) {
        options.valid = false;
        options.errorMessage =
            QStringLiteral("Unknown action: %1").arg(parser.value(actionOption));
        return options;
    }

    const QString scopeValue = parser.value(diffScopeOption).trimmed().toLower();
    if (scopeValue == QLatin1String("staged")) {
        options.diffScope = WorkingDiffScope::Staged;
    } else if (scopeValue == QLatin1String("head")) {
        options.diffScope = WorkingDiffScope::AgainstHead;
    } else if (scopeValue == QLatin1String("unstaged")) {
        options.diffScope = WorkingDiffScope::Unstaged;
    } else {
        options.valid = false;
        options.errorMessage = QStringLiteral("Unknown diff scope: %1 (use unstaged, staged, or head)")
                                   .arg(parser.value(diffScopeOption));
        return options;
    }

    options.filePaths = parser.values(fileOption);
    options.inputPaths = parser.positionalArguments();

    resolvePaths(&options, git);
    return options;
}
