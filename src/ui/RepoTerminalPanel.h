#pragma once

#include <QWidget>

class QLabel;
class VtermTerminalWidget;

class RepoTerminalPanel : public QWidget {
    Q_OBJECT

public:
    explicit RepoTerminalPanel(QWidget *parent = nullptr);

    void setWorkingDirectory(const QString &repoPath);
    void focusInput();

signals:
    void repositoryMayHaveChanged();

private:
    void onShellExited(int exitCode);

    QString m_repoRoot;
    QLabel *m_cwdLabel = nullptr;
    VtermTerminalWidget *m_terminal = nullptr;
};
