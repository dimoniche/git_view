#include "ui/PtySession.h"

#include <QFileInfo>
#include <QSocketNotifier>
#include <QtGlobal>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

namespace {

QString defaultShell()
{
    const QByteArray shell = qgetenv("SHELL");
    if (!shell.isEmpty()) {
        return QString::fromLocal8Bit(shell);
    }
    return QStringLiteral("/bin/bash");
}

} // namespace

PtySession::PtySession(QObject *parent)
    : QObject(parent)
{
}

PtySession::~PtySession()
{
    stop();
}

void PtySession::stop()
{
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }

    if (m_masterFd >= 0) {
        close(m_masterFd);
        m_masterFd = -1;
    }

    if (m_childPid > 0) {
        kill(m_childPid, SIGHUP);
        int status = 0;
        waitpid(m_childPid, &status, WNOHANG);
        m_childPid = -1;
    }

    m_running = false;
}

bool PtySession::start(const QString &workingDirectory, const QString &shellProgram)
{
    stop();

    const QString shell = shellProgram.isEmpty() ? defaultShell() : shellProgram;
    const QByteArray shellBytes = QFile::encodeName(shell);

    struct winsize ws{};
    ws.ws_col = 80;
    ws.ws_row = 24;

    m_childPid = forkpty(&m_masterFd, nullptr, nullptr, &ws);
    if (m_childPid < 0) {
        return false;
    }

    if (m_childPid == 0) {
        setsid();

        const QString cwd = QFileInfo(workingDirectory).canonicalFilePath();
        if (!cwd.isEmpty()) {
            chdir(QFile::encodeName(cwd).constData());
        }

        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);

        execl(shellBytes.constData(), shellBytes.constData(), "-l", nullptr);
        _exit(127);
    }

    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &PtySession::onReadyRead);

    m_running = true;
    return true;
}

void PtySession::write(const QByteArray &data)
{
    if (!m_running || m_masterFd < 0 || data.isEmpty()) {
        return;
    }

    ssize_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = ::write(m_masterFd, data.constData() + offset,
                                        static_cast<size_t>(data.size() - offset));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EPIPE) {
                onChildExited();
            }
            break;
        }
        offset += written;
    }
}

void PtySession::resize(int columns, int rows)
{
    if (!m_running || m_masterFd < 0 || columns < 1 || rows < 1) {
        return;
    }

    struct winsize ws{};
    ws.ws_col = static_cast<unsigned short>(columns);
    ws.ws_row = static_cast<unsigned short>(rows);
    ioctl(m_masterFd, TIOCSWINSZ, &ws);
}

void PtySession::onReadyRead()
{
    if (m_masterFd < 0) {
        return;
    }

    char buffer[4096];
    const ssize_t bytes = ::read(m_masterFd, buffer, sizeof(buffer));
    if (bytes > 0) {
        emit readyRead(QByteArray(buffer, static_cast<int>(bytes)));
        return;
    }

    if (bytes == 0) {
        onChildExited();
    }
}

void PtySession::onChildExited()
{
    if (!m_running) {
        return;
    }

    int status = 0;
    if (m_childPid > 0) {
        waitpid(m_childPid, &status, WNOHANG);
        m_childPid = -1;
    }

    stop();
    emit exited(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}
