#include "ui/CommitHistoryView.h"

#include <QColor>
#include <QContextMenuEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QClipboard>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <QSizePolicy>

namespace {

QColor laneColor(int lane)
{
    static const QColor palette[] = {
        QColor(0x20, 0x99, 0x6b),
        QColor(0x35, 0x7a, 0xbd),
        QColor(0xc0, 0x5a, 0x1a),
        QColor(0x8e, 0x44, 0xad),
        QColor(0xc0, 0x39, 0x2b),
        QColor(0x16, 0xa0, 0x85),
        QColor(0xd4, 0xac, 0x0d),
        QColor(0x7f, 0x8c, 0x8d),
    };
    return palette[lane % (sizeof(palette) / sizeof(palette[0]))];
}

void drawEdge(QPainter &painter, const GraphEdge &edge, int headerHeight)
{
    constexpr int rowHeight = 34;
    constexpr int laneWidth = 18;
    constexpr int graphPadding = 10;

    const int fromY = headerHeight + edge.fromRow * rowHeight + rowHeight / 2;
    const int toY = headerHeight + edge.toRow * rowHeight + rowHeight / 2;
    const int fromX = graphPadding + edge.fromLane * laneWidth + laneWidth / 2;
    const int toX = graphPadding + edge.toLane * laneWidth + laneWidth / 2;

    QPen pen(QColor(0xbb, 0xbb, 0xbb));
    pen.setWidth(2);
    painter.setPen(pen);

    if (edge.fromRow + 1 == edge.toRow) {
        painter.drawLine(fromX, fromY, toX, toY);
        return;
    }

    const int midY = (fromY + toY) / 2;
    painter.drawLine(fromX, fromY, fromX, midY);
    if (fromX != toX) {
        painter.drawLine(fromX, midY, toX, midY);
    }
    painter.drawLine(toX, midY, toX, toY);
}

} // namespace

CommitHistoryView::CommitHistoryView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QFont viewFont = font();
    if (viewFont.pointSize() > 0) {
        viewFont.setPointSize(viewFont.pointSize() + 1);
    } else {
        viewFont.setPixelSize(14);
    }
    setFont(viewFont);
}

void CommitHistoryView::setCommits(const std::vector<Commit> &commits)
{
    m_commits = commits;
    m_layout = GraphLayout::build(m_commits);
    m_selectedRow = m_commits.empty() ? -1 : 0;
    updateContentGeometry();
    update();
    if (!m_commits.empty()) {
        emit commitSelected(m_commits.front().hash);
    }
}

QString CommitHistoryView::selectedHash() const
{
    if (m_selectedRow < 0 || m_selectedRow >= static_cast<int>(m_commits.size())) {
        return {};
    }
    return m_commits[static_cast<size_t>(m_selectedRow)].hash;
}

int CommitHistoryView::contentHeight() const
{
    return kHeaderHeight + static_cast<int>(m_commits.size()) * kRowHeight;
}

void CommitHistoryView::updateContentGeometry()
{
    const int contentH = std::max(contentHeight(), height());
    setMinimumHeight(contentH);
    updateGeometry();
}

QSize CommitHistoryView::sizeHint() const
{
    const int w = graphWidth() + kTextLeftPad + kHashWidth + kAuthorWidth + kDateWidth + 400;
    return {w, std::max(contentHeight(), 400)};
}

QSize CommitHistoryView::minimumSizeHint() const
{
    return {600, std::max(contentHeight(), 300)};
}

int CommitHistoryView::graphWidth() const
{
    return kGraphPadding * 2 + std::max(1, m_layout.laneCount) * kLaneWidth;
}

int CommitHistoryView::rowY(int row) const
{
    return kHeaderHeight + row * kRowHeight;
}

QRect CommitHistoryView::graphRect() const
{
    return {0, kHeaderHeight, graphWidth(), static_cast<int>(m_commits.size()) * kRowHeight};
}

void CommitHistoryView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateContentGeometry();
}

void CommitHistoryView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Base));

    const int textX = graphWidth() + kTextLeftPad;
    const QFontMetrics fm(painter.font());
    const QFont headerFont = [](const QFont &base) {
        QFont font = base;
        font.setBold(true);
        return font;
    }(painter.font());

    painter.fillRect(0, 0, width(), kHeaderHeight, palette().color(QPalette::Midlight));
    painter.setFont(headerFont);
    painter.setPen(palette().color(QPalette::WindowText));

    int colX = textX;
    painter.drawText(colX, 0, kHashWidth, kHeaderHeight, Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Hash"));
    colX += kHashWidth;
    painter.drawText(colX, 0, kAuthorWidth, kHeaderHeight, Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Author"));
    colX += kAuthorWidth;
    painter.drawText(colX, 0, kDateWidth, kHeaderHeight, Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Date"));
    colX += kDateWidth;
    painter.drawText(colX, 0, width() - colX - 8, kHeaderHeight, Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Subject"));

    painter.setFont(font());
    painter.drawLine(0, kHeaderHeight - 1, width(), kHeaderHeight - 1);

    if (m_commits.empty()) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRect(0, kHeaderHeight, width(), height() - kHeaderHeight),
                         Qt::AlignCenter, tr("No commits"));
        return;
    }

    const QRect graphArea = graphRect();
    painter.setClipRect(graphArea);
    for (const GraphEdge &edge : m_layout.edges) {
        drawEdge(painter, edge, kHeaderHeight);
    }
    painter.setClipping(false);

    for (size_t row = 0; row < m_commits.size(); ++row) {
        const int y = rowY(static_cast<int>(row));
        const QRect rowRect(0, y, width(), kRowHeight);

        if (static_cast<int>(row) == m_selectedRow) {
            painter.fillRect(rowRect, palette().color(QPalette::Highlight));
        } else if (row % 2 == 1) {
            painter.fillRect(rowRect, palette().color(QPalette::AlternateBase));
        }

        const int lane = m_layout.lanes[row];
        const int cx = kGraphPadding + lane * kLaneWidth + kLaneWidth / 2;
        const int cy = y + kRowHeight / 2;
        const QColor color = laneColor(lane);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        const int radius = m_commits[row].isMerge() ? 7 : 6;
        painter.drawEllipse(QPoint(cx, cy), radius, radius);

        const Commit &commit = m_commits[row];
        const bool selected = static_cast<int>(row) == m_selectedRow;
        const QColor textColor =
            selected ? palette().color(QPalette::HighlightedText)
                     : palette().color(QPalette::Text);
        painter.setPen(textColor);

        colX = textX;
        const QString hash = commit.hash.left(8);
        painter.drawText(colX, y, kHashWidth, kRowHeight, Qt::AlignLeft | Qt::AlignVCenter, hash);
        colX += kHashWidth;

        const QString author = fm.elidedText(commit.author, Qt::ElideRight, kAuthorWidth);
        painter.drawText(colX, y, kAuthorWidth, kRowHeight, Qt::AlignLeft | Qt::AlignVCenter,
                         author);
        colX += kAuthorWidth;

        const QString date = fm.elidedText(commit.date, Qt::ElideRight, kDateWidth);
        painter.drawText(colX, y, kDateWidth, kRowHeight, Qt::AlignLeft | Qt::AlignVCenter, date);
        colX += kDateWidth;

        const int subjectWidth = width() - colX - 8;
        const QString subject =
            fm.elidedText(commit.subject, Qt::ElideRight, std::max(80, subjectWidth));
        painter.drawText(colX, y, subjectWidth, kRowHeight, Qt::AlignLeft | Qt::AlignVCenter,
                         subject);
    }
}

void CommitHistoryView::selectRow(int row)
{
    if (row < 0 || row >= static_cast<int>(m_commits.size()) || row == m_selectedRow) {
        return;
    }
    m_selectedRow = row;
    update();
    emit commitSelected(m_commits[static_cast<size_t>(row)].hash);
}

void CommitHistoryView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        const int row = rowAtY(static_cast<int>(event->position().y()));
        if (row >= 0 && row < static_cast<int>(m_commits.size())) {
            selectRow(row);
            showCommitContextMenu(row, event->globalPosition().toPoint());
        }
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const int row = rowAtY(static_cast<int>(event->position().y()));
    if (row < 0 || row >= static_cast<int>(m_commits.size())) {
        return;
    }
    selectRow(row);
}

void CommitHistoryView::contextMenuEvent(QContextMenuEvent *event)
{
    const int row = rowAtY(event->pos().y());
    if (row < 0 || row >= static_cast<int>(m_commits.size())) {
        return;
    }
    selectRow(row);
    showCommitContextMenu(row, event->globalPos());
}

void CommitHistoryView::showCommitContextMenu(int row, const QPoint &globalPos)
{
    const Commit &commit = m_commits[static_cast<size_t>(row)];

    QMenu menu(this);

    menu.addAction(tr("Show commit details"), this, [this, commit]() {
        emit viewCommitDetailsRequested(commit.hash);
    });

    menu.addSeparator();

    menu.addAction(tr("Copy full hash"), this, [commit]() {
        if (QClipboard *clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(commit.hash);
        }
    });
    menu.addAction(tr("Copy short hash"), this, [commit]() {
        if (QClipboard *clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(commit.hash.left(8));
        }
    });
    menu.addAction(tr("Copy subject"), this, [commit]() {
        if (QClipboard *clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(commit.subject);
        }
    });

    menu.addSeparator();
    menu.addAction(tr("Create branch from commit…"), this, [this, commit]() {
        emit createBranchFromCommitRequested(commit.hash);
    });

    menu.exec(globalPos);
}

int CommitHistoryView::rowAtY(int y) const
{
    if (y < kHeaderHeight) {
        return -1;
    }
    const int row = (y - kHeaderHeight) / kRowHeight;
    if (row < 0 || row >= static_cast<int>(m_commits.size())) {
        return -1;
    }
    return row;
}
