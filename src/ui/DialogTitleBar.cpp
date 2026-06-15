#include "ui/DialogTitleBar.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QToolButton>

DialogTitleBar::DialogTitleBar(const QString &title, QWidget *window, QWidget *parent)
    : QWidget(parent), m_window(window)
{
    setFixedHeight(32);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 4, 0);
    layout->setSpacing(4);

    auto *label = new QLabel(title, this);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    layout->addWidget(label);

    auto *closeButton = new QToolButton(this);
    closeButton->setText(QStringLiteral("✕"));
    closeButton->setAutoRaise(true);
    closeButton->setFixedSize(28, 28);
    connect(closeButton, &QToolButton::clicked, window, &QWidget::close);
    layout->addWidget(closeButton);

    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, pal.color(QPalette::Midlight));
    setPalette(pal);
}

void DialogTitleBar::toggleMaximized()
{
    if (!m_window) {
        return;
    }

    if (m_window->isMaximized()) {
        m_window->showNormal();
    } else {
        m_window->showMaximized();
    }
}

void DialogTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        toggleMaximized();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void DialogTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_window && !m_window->isMaximized()) {
        m_dragOffset = event->globalPosition().toPoint() - m_window->frameGeometry().topLeft();
        m_dragging = true;
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void DialogTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && m_window && !m_window->isMaximized()) {
        m_window->move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void DialogTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}
