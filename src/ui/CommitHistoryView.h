#pragma once

#include "core/Commit.h"
#include "core/GraphLayout.h"

#include <QWidget>
#include <vector>

class CommitHistoryView : public QWidget {
    Q_OBJECT

public:
    explicit CommitHistoryView(QWidget *parent = nullptr);

    void setCommits(const std::vector<Commit> &commits);
    int selectedRow() const { return m_selectedRow; }
    QString selectedHash() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void commitSelected(const QString &hash);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateContentGeometry();
    int rowAtY(int y) const;
    int graphWidth() const;
    int contentHeight() const;
    int rowY(int row) const;
    QRect graphRect() const;

    std::vector<Commit> m_commits;
    GraphLayout m_layout;
    int m_selectedRow = -1;

    static constexpr int kRowHeight = 34;
    static constexpr int kHeaderHeight = 32;
    static constexpr int kLaneWidth = 18;
    static constexpr int kGraphPadding = 10;
    static constexpr int kTextLeftPad = 12;
    static constexpr int kHashWidth = 80;
    static constexpr int kAuthorWidth = 140;
    static constexpr int kDateWidth = 170;
};
