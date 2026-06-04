#pragma once

#include <QObject>
#include <QString>

class QSocketNotifier;

class PtySession : public QObject {
    Q_OBJECT

public:
    explicit PtySession(QObject *parent = nullptr);
    ~PtySession() override;

    bool start(const QString &workingDirectory, const QString &shellProgram = {});
    void stop();
    void write(const QByteArray &data);
    void resize(int columns, int rows);
    bool isRunning() const { return m_running; }

signals:
    void readyRead(const QByteArray &data);
    void exited(int exitCode);

private:
    void onReadyRead();
    void onChildExited();

    int m_masterFd = -1;
    int m_childPid = -1;
    bool m_running = false;
    QSocketNotifier *m_readNotifier = nullptr;
};
