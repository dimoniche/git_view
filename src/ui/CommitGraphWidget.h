#pragma once

#include "core/Commit.h"
#include "core/GraphLayout.h"

#include <QWidget>
#include <vector>

class CommitGraphWidget : public QWidget {
    Q_OBJECT

public:
    explicit CommitGraphWidget(QWidget *parent = nullptr);

    void setData(const std::vector<Commit> &commits, const GraphLayout &layout);
    void setRowHeight(int height);
    void setSelectedRow(int row);
    int naturalWidth() const;
    int graphLanesWidth() const;
    int labelAreaWidth() const;
    int rowHeight() const { return m_rowHeight; }

    QSize sizeHint() const override;

signals:
    void rowClicked(int row);
    void rowDoubleClicked(int row);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int laneCenterX(int lane) const;
    int rowY(int row) const;
    int rowAtY(int y) const;

    std::vector<Commit> m_commits;
    GraphLayout m_layout;
    int m_selectedRow = -1;
    int m_rowHeight = 34;

    static constexpr int kLaneWidth = 18;
    static constexpr int kGraphPadding = 10;
    static constexpr int kLabelPadding = 6;
};
