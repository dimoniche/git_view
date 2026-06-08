#pragma once

#include "git/GitService.h"

#include <QString>
#include <QStringList>

enum class LaunchAction {
    Open,
    WorkingChanges,
    FileHistory,
    FileDiff,
    Commit,
    Log,
};

struct AppLaunchOptions {
    QString repoPath;
    LaunchAction action = LaunchAction::Open;
    QStringList inputPaths;
    QStringList filePaths;
    WorkingDiffScope diffScope = WorkingDiffScope::Unstaged;
    bool showHelp = false;
    bool valid = true;
    QString errorMessage;

    static AppLaunchOptions parse(const QStringList &arguments, GitService *git = nullptr);
    static QString helpText();
    static LaunchAction actionFromString(const QString &value, bool *ok = nullptr);
    static QString actionToString(LaunchAction action);
};
