#include "ui/DiffViewerDialog.h"

#include "ui/DiffViewerWidget.h"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QShortcut>
#include <QSizeGrip>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

class DiffViewerTitleBar : public QWidget {
public:
    explicit DiffViewerTitleBar(const QString &title, QWidget *window, QWidget *parent = nullptr);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void toggleMaximized();

    QWidget *m_window = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
};

DiffViewerTitleBar::DiffViewerTitleBar(const QString &title, QWidget *window, QWidget *parent)
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

void DiffViewerTitleBar::toggleMaximized()
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

void DiffViewerTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        toggleMaximized();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void DiffViewerTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_window && !m_window->isMaximized()) {
        m_dragOffset = event->globalPosition().toPoint() - m_window->frameGeometry().topLeft();
        m_dragging = true;
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void DiffViewerTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && m_window && !m_window->isMaximized()) {
        m_window->move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void DiffViewerTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

} // namespace

DiffViewerDialog::DiffViewerDialog(const QString &title, const QString &diff, QWidget *parent,
                                   const DiffViewerSources &sources)
    : QWidget(parent)
{
    setWindowTitle(title);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(480, 320);
    resize(1200, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(new DiffViewerTitleBar(title, this));

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    auto *viewer = new DiffViewerWidget(content);
    viewer->setDiff(diff);
    if (!sources.before.isEmpty() || !sources.after.isEmpty()) {
        viewer->setSources(sources.before, sources.after, sources.beforeCaption,
                           sources.afterCaption);
    }

    contentLayout->addWidget(viewer, 1);

    auto *gripRow = new QHBoxLayout();
    gripRow->addStretch();
    gripRow->addWidget(new QSizeGrip(content));
    contentLayout->addLayout(gripRow);

    layout->addWidget(content, 1);

    auto *closeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    closeShortcut->setContext(Qt::WindowShortcut);
    connect(closeShortcut, &QShortcut::activated, this, &QWidget::close);
}

void DiffViewerDialog::showDiff(QWidget *parent, const QString &title, const QString &text,
                                const DiffViewerSources &sources)
{
    auto *dialog = new DiffViewerDialog(title, text, parent, sources);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void DiffViewerDialog::showSource(QWidget *parent, const QString &title, const QString &text)
{
    auto *dialog = new DiffViewerDialog(title, text, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
