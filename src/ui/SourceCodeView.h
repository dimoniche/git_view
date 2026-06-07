#pragma once

#include <QPlainTextEdit>

class QKeyEvent;
class QWheelEvent;

class SourceCodeView : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit SourceCodeView(QWidget *parent = nullptr);

    void setScrollPartner(SourceCodeView *partner);
    void setScrollSyncMuted(bool muted);

    void revealDisplayLineCentered(int displayLine);
    int verticalScrollValue() const;
    void setVerticalScrollValue(int value);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void schedulePartnerSync();
    void syncPartnerNow();

    SourceCodeView *m_scrollPartner = nullptr;
    bool m_syncingScroll = false;
    bool m_scrollSyncMuted = false;
};
