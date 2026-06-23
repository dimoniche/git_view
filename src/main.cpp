#include "AppLaunchOptions.h"
#include "git_view_version.h"
#include "git/GitService.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QTimer>
#include <QTextStream>

#include <cstring>

#if defined(Q_OS_UNIX)
#include <signal.h>
#endif

namespace {

bool argumentsAskForHelp(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    if (argumentsAskForHelp(argc, argv)) {
        QTextStream(stdout) << AppLaunchOptions::helpText() << '\n';
        return 0;
    }

#if defined(Q_OS_UNIX)
    signal(SIGPIPE, SIG_IGN);
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("git_view"));
    QApplication::setOrganizationName(QStringLiteral("git_view"));
    QApplication::setApplicationVersion(QStringLiteral(GIT_VIEW_VERSION));

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QGuiApplication::setDesktopFileName(QStringLiteral("git_view"));
#endif

    const QIcon appIcon(QStringLiteral(":/icons/git_view.png"));
    if (!appIcon.isNull()) {
        QApplication::setWindowIcon(appIcon);
    }

    GitService git;
    const AppLaunchOptions options = AppLaunchOptions::parse(QCoreApplication::arguments(), &git);

    if (!options.valid) {
        QTextStream(stderr) << options.errorMessage << '\n';
        return 1;
    }

    MainWindow window;
    window.show();
    QTimer::singleShot(0, &window, [&window, options]() { window.applyLaunchOptions(options); });

    return app.exec();
}
