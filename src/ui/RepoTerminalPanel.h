#pragma once

#include <QProcess>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;

class RepoTerminalPanel : public QWidget {
    Q_OBJECT

public:
    explicit RepoTerminalPanel(QWidget *parent = nullptr);

    void setWorkingDirectory(const QString &repoPath);
    QString workingDirectory() const { return m_cwd; }
    void focusInput();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void repositoryMayHaveChanged();

private slots:
    void runCurrentCommand();
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void appendOutput(const QString &text, bool isError = false);
    bool applyChangeDirectory(const QString &command);
    bool commandLikelyChangesRepository(const QString &command) const;
    void setInputEnabled(bool enabled);

    QString m_repoRoot;
    QString m_cwd;
    QProcess *m_process = nullptr;
    QPlainTextEdit *m_output = nullptr;
    QLineEdit *m_input = nullptr;
    QLabel *m_cwdLabel = nullptr;
    QStringList m_history;
    int m_historyIndex = -1;
    QString m_savedInput;
};
