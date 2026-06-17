#pragma once

#include <QTimer>
#include <QVector>
#include <QWidget>

#include <vterm.h>

class PtySession;
class QKeyEvent;

class VtermTerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit VtermTerminalWidget(QWidget *parent = nullptr);
    ~VtermTerminalWidget() override;

    bool startShell(const QString &workingDirectory);
    void stopShell();
    bool isRunning() const;
    void syncDisplaySize();
    void setGridSize(int rows, int columns);

signals:
    void shellExited(int exitCode);

protected:
    QSize sizeHint() const override;
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    static int onSbPushLine(int cols, const VTermScreenCell *cells, void *user);
    static int onSbPopLine(int cols, VTermScreenCell *cells, void *user);
    static int onSbClear(void *user);
    static int onMoveCursor(VTermPos pos, VTermPos oldpos, int visible, void *user);
    static int onSetTermProp(VTermProp prop, VTermValue *val, void *user);

    void onPtyReadyRead(const QByteArray &data);
    void onPtyExited(int exitCode);
    void updateTerminalSize(bool force = false);
    void sendKey(VTermKey key, VTermModifier mod);
    void sendInputToPty(const QByteArray &data);
    bool sendControlCharacter(QKeyEvent *event);
    void clearScrollback();
    bool scrollbackPush(int cols, const VTermScreenCell *cells);
    bool scrollbackPop(int cols, VTermScreenCell *cells);
    bool cellAtDisplayLine(int displayLine, int col, VTermScreenCell *cell) const;
    void clampScrollOffset();
    void paintCursor(QPainter &painter) const;

    PtySession *m_pty = nullptr;
    VTerm *m_vterm = nullptr;
    VTermScreen *m_vtermScreen = nullptr;
    int m_cols = 80;
    int m_rows = 24;
    int m_cellWidth = 8;
    int m_cellHeight = 16;
    bool m_hasFocus = false;
    int m_cursorRow = 0;
    int m_cursorCol = 0;
    bool m_cursorVisible = true;
    bool m_cursorBlink = false;
    bool m_cursorBlinkPhase = true;
    int m_cursorShape = VTERM_PROP_CURSORSHAPE_BLOCK;
    QTimer *m_cursorBlinkTimer = nullptr;
    QVector<QVector<VTermScreenCell>> m_scrollback;
    int m_scrollOffset = 0;
    static constexpr int kMaxScrollbackLines = 10000;
};
