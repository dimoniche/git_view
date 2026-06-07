#include "ui/SourceCodeView.h"

#include <QAbstractSlider>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTimer>
#include <QWheelEvent>

SourceCodeView::SourceCodeView(QWidget *parent)
    : QPlainTextEdit(parent)
{
    viewport()->installEventFilter(this);

    QScrollBar *bar = verticalScrollBar();
    connect(bar, &QScrollBar::valueChanged, this, [this](int) { schedulePartnerSync(); });
    connect(bar, &QScrollBar::sliderMoved, this, [this](int) { schedulePartnerSync(); });
    connect(bar, &QScrollBar::actionTriggered, this, [this](int) { schedulePartnerSync(); });
}

void SourceCodeView::setScrollPartner(SourceCodeView *partner)
{
    m_scrollPartner = partner;
}

void SourceCodeView::setScrollSyncMuted(bool muted)
{
    m_scrollSyncMuted = muted;
}

int SourceCodeView::verticalScrollValue() const
{
    return verticalScrollBar()->value();
}

void SourceCodeView::setVerticalScrollValue(int value)
{
    QScrollBar *bar = verticalScrollBar();
    bar->setValue(qBound(bar->minimum(), value, bar->maximum()));
}

void SourceCodeView::revealDisplayLineCentered(int displayLine)
{
    if (displayLine < 1) {
        return;
    }

    const QTextBlock block = document()->findBlockByNumber(displayLine - 1);
    if (!block.isValid()) {
        return;
    }

    m_syncingScroll = true;
    if (m_scrollPartner) {
        m_scrollPartner->m_syncingScroll = true;
    }

    QTextCursor cursor(block);
    cursor.clearSelection();
    setTextCursor(cursor);
    centerCursor();

    if (m_scrollPartner) {
        const QTextBlock partnerBlock =
            m_scrollPartner->document()->findBlockByNumber(displayLine - 1);
        if (partnerBlock.isValid()) {
            QTextCursor partnerCursor(partnerBlock);
            partnerCursor.clearSelection();
            m_scrollPartner->setTextCursor(partnerCursor);
            m_scrollPartner->centerCursor();
        }
    }

    if (m_scrollPartner) {
        m_scrollPartner->m_syncingScroll = false;
    }
    m_syncingScroll = false;
}

int SourceCodeView::partnerScrollValueFor(int localScrollValue) const
{
    if (!m_scrollPartner) {
        return localScrollValue;
    }

    const QScrollBar *bar = verticalScrollBar();
    const QScrollBar *partnerBar = m_scrollPartner->verticalScrollBar();
    if (bar->maximum() == partnerBar->maximum()) {
        return localScrollValue;
    }

    const qreal ratio = bar->maximum() > 0
                            ? static_cast<qreal>(localScrollValue) / static_cast<qreal>(bar->maximum())
                            : 0.0;
    return qRound(ratio * static_cast<qreal>(partnerBar->maximum()));
}

void SourceCodeView::pushScrollToPartner(int localScrollValue)
{
    if (!m_scrollPartner) {
        return;
    }

    const int partnerValue = partnerScrollValueFor(localScrollValue);
    m_scrollPartner->setVerticalScrollValue(partnerValue);
}

void SourceCodeView::schedulePartnerSync()
{
    if (m_scrollSyncMuted || m_syncingScroll) {
        return;
    }

    QTimer::singleShot(0, this, [this]() { syncPartnerNow(); });
}

void SourceCodeView::syncPartnerNow()
{
    if (m_scrollSyncMuted || m_syncingScroll || !m_scrollPartner) {
        return;
    }

    QScrollBar *bar = verticalScrollBar();
    QScrollBar *partnerBar = m_scrollPartner->verticalScrollBar();
    const int value = bar->value();
    const int partnerValue = partnerScrollValueFor(value);

    if (partnerBar->value() == partnerValue) {
        return;
    }

    m_syncingScroll = true;
    m_scrollPartner->m_syncingScroll = true;
    m_scrollPartner->setVerticalScrollValue(partnerValue);
    m_scrollPartner->m_syncingScroll = false;
    m_syncingScroll = false;
}

bool SourceCodeView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == viewport()) {
        switch (event->type()) {
        case QEvent::Wheel:
        case QEvent::KeyPress:
            schedulePartnerSync();
            break;
        default:
            break;
        }
    }

    return QPlainTextEdit::eventFilter(watched, event);
}

void SourceCodeView::keyPressEvent(QKeyEvent *event)
{
    QPlainTextEdit::keyPressEvent(event);
    schedulePartnerSync();
}

void SourceCodeView::wheelEvent(QWheelEvent *event)
{
    QPlainTextEdit::wheelEvent(event);
    schedulePartnerSync();
}
