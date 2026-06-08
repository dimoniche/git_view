#pragma once

#include <QWidget>
#include <QString>

class DialogTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit DialogTitleBar(const QString &title, QWidget *window, QWidget *parent = nullptr);

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
