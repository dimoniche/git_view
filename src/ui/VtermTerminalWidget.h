#pragma once

#include <QVector>
#include <QWidget>

#include <vterm.h>

class PtySession;

class VtermTerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit VtermTerminalWidget(QWidget *parent = nullptr);
    ~VtermTerminalWidget() override;

    bool startShell(const QString &workingDirectory);
    void stopShell();
    bool isRunning() const;
    void setGridSize(int rows, int columns);

signals:
    void shellExited(int exitCode);

protected:
    QSize sizeHint() const override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    static int onSbPushLine(int cols, const VTermScreenCell *cells, void *user);
    static int onSbPopLine(int cols, VTermScreenCell *cells, void *user);
    static int onSbClear(void *user);

    void onPtyReadyRead(const QByteArray &data);
    void onPtyExited(int exitCode);
    void updateTerminalSize();
    void sendKey(VTermKey key, VTermModifier mod);
    void clearScrollback();
    bool scrollbackPush(int cols, const VTermScreenCell *cells);
    bool scrollbackPop(int cols, VTermScreenCell *cells);
    bool cellAtDisplayLine(int displayLine, int col, VTermScreenCell *cell) const;
    void clampScrollOffset();

    PtySession *m_pty = nullptr;
    VTerm *m_vterm = nullptr;
    VTermScreen *m_vtermScreen = nullptr;
    int m_cols = 80;
    int m_rows = 24;
    int m_cellWidth = 8;
    int m_cellHeight = 16;
    bool m_hasFocus = false;
    QVector<QVector<VTermScreenCell>> m_scrollback;
    int m_scrollOffset = 0;
    static constexpr int kMaxScrollbackLines = 10000;
};
