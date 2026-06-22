#include "ui/CommitGraphWidget.h"

#include "ui/EditorTheme.h"

#include <QHash>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <functional>
#include <utility>

namespace {

struct TagLabelStyle {
    QColor text;
    QColor background;
};

TagLabelStyle tagLabelStyle(const QWidget *widget, bool selected)
{
    const bool dark = editorUsesDarkTheme(widget);
    if (selected) {
        return {QColor(0xff, 0xff, 0xff), QColor(0, 0, 0, 0)};
    }
    if (dark) {
        return {QColor(0xff, 0xd5, 0x4f), QColor(0x92, 0x40, 0x0e, 180)};
    }
    return {QColor(0x92, 0x40, 0x0e), QColor(0xff, 0xed, 0xd5)};
}

int labelTextWidth(const QFontMetrics &fm, const QString &text)
{
    if (text.isEmpty()) {
        return 0;
    }
    return qMax(fm.horizontalAdvance(text), fm.boundingRect(text).width()) + 4;
}

void drawTagLabel(QPainter &painter, const QWidget *widget, const QRect &rect, const QString &text,
                  bool selected)
{
    if (text.isEmpty()) {
        return;
    }

    const TagLabelStyle style = tagLabelStyle(widget, selected);

    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);

    const QFontMetrics fm(font);
    const int textWidth = labelTextWidth(fm, text);
    const int pillHeight = fm.height() + 4;
    const QRect pillRect(rect.left(), rect.top() + (rect.height() - pillHeight) / 2,
                         textWidth + 6, pillHeight);

    if (style.background.alpha() > 0) {
        painter.fillRect(pillRect, style.background);
    }

    painter.setPen(style.text);
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, text);
}

struct RowGraphLabels {
    QString branchText;
    QString tagText;
};

QHash<int, RowGraphLabels> collectRowLabels(const GraphLayout &layout)
{
    QHash<int, RowGraphLabels> byRow;
    for (const GraphLaneLabel &label : layout.laneLabels) {
        if (label.lane <= 0 || label.name.isEmpty()) {
            continue;
        }
        RowGraphLabels &rowLabels = byRow[label.row];
        if (rowLabels.branchText.isEmpty()) {
            rowLabels.branchText = label.name;
        } else if (!rowLabels.branchText.contains(label.name)) {
            rowLabels.branchText += QStringLiteral(", ") + label.name;
        }
    }
    for (const GraphRowLabel &label : layout.rowLabels) {
        if (label.name.isEmpty()) {
            continue;
        }
        RowGraphLabels &rowLabels = byRow[label.row];
        if (label.kind == GraphRefKind::Tag) {
            rowLabels.tagText = label.name;
        } else if (rowLabels.branchText.isEmpty()) {
            rowLabels.branchText = label.name;
        }
    }
    return byRow;
}

int combinedLabelWidth(const QFont &font, const RowGraphLabels &labels)
{
    const QFontMetrics fm(font);
    QFont tagFont = font;
    tagFont.setBold(true);
    const QFontMetrics tagFm(tagFont);

    int width = 0;
    if (!labels.branchText.isEmpty()) {
        width += labelTextWidth(fm, labels.branchText);
    }
    if (!labels.branchText.isEmpty() && !labels.tagText.isEmpty()) {
        width += labelTextWidth(fm, QStringLiteral("  "));
    }
    if (!labels.tagText.isEmpty()) {
        width += labelTextWidth(tagFm, labels.tagText) + 6;
    }
    return width;
}

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

void drawEdge(QPainter &painter,
              const GraphEdge &edge,
              int rowHeight,
              const std::function<int(int)> &laneCenterX)
{
    const int fromY = edge.fromRow * rowHeight + rowHeight / 2;
    const int toY = edge.toRow * rowHeight + rowHeight / 2;
    const int fromX = laneCenterX(edge.fromLane);
    const int toX = laneCenterX(edge.toLane);

    QPen pen(QColor(0xbb, 0xbb, 0xbb));
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

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

CommitGraphWidget::CommitGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void CommitGraphWidget::setData(const std::vector<Commit> &commits, const GraphLayout &layout)
{
    m_commits = commits;
    m_layout = layout;
    update();
}

void CommitGraphWidget::setRowHeight(int height)
{
    if (height > 0 && height != m_rowHeight) {
        m_rowHeight = height;
        updateGeometry();
        update();
    }
}

void CommitGraphWidget::setSelectedRow(int row)
{
    m_selectedRow = row;
    update();
}

int CommitGraphWidget::graphLanesWidth() const
{
    const int lanes = std::max(1, m_layout.laneCount);
    return kGraphPadding * 2 + lanes * kLaneWidth;
}

int CommitGraphWidget::labelAreaWidth() const
{
    const QHash<int, RowGraphLabels> byRow = collectRowLabels(m_layout);
    if (byRow.isEmpty()) {
        return 0;
    }

    int maxWidth = 0;
    for (auto it = byRow.constBegin(); it != byRow.constEnd(); ++it) {
        maxWidth = std::max(maxWidth, combinedLabelWidth(font(), it.value()));
    }
    return maxWidth + kLabelPadding;
}

int CommitGraphWidget::naturalWidth() const
{
    return graphLanesWidth() + labelAreaWidth();
}

QSize CommitGraphWidget::sizeHint() const
{
    const int rows = static_cast<int>(m_commits.size());
    return {naturalWidth(), rows * m_rowHeight};
}

int CommitGraphWidget::laneCenterX(int lane) const
{
    return kGraphPadding + lane * kLaneWidth + kLaneWidth / 2;
}

int CommitGraphWidget::rowY(int row) const
{
    return row * m_rowHeight;
}

int CommitGraphWidget::rowAtY(int y) const
{
    if (y < 0) {
        return -1;
    }
    const int row = y / m_rowHeight;
    if (row < 0 || row >= static_cast<int>(m_commits.size())) {
        return -1;
    }
    return row;
}

void CommitGraphWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().color(QPalette::Base));

    if (m_commits.empty()) {
        return;
    }

    const QRect graphArea(0, 0, width(), static_cast<int>(m_commits.size()) * m_rowHeight);
    const auto laneX = [this](int lane) { return laneCenterX(lane); };

    for (size_t row = 0; row < m_commits.size(); ++row) {
        if (static_cast<int>(row) == m_selectedRow) {
            painter.fillRect(0, rowY(static_cast<int>(row)), width(), m_rowHeight,
                             palette().color(QPalette::Highlight));
        } else if (row % 2 == 1) {
            painter.fillRect(0, rowY(static_cast<int>(row)), width(), m_rowHeight,
                             palette().color(QPalette::AlternateBase));
        }
    }

    painter.setClipRect(graphArea);
    for (const GraphEdge &edge : m_layout.edges) {
        drawEdge(painter, edge, m_rowHeight, laneX);
    }
    painter.setClipping(false);

    for (size_t row = 0; row < m_commits.size(); ++row) {
        const int y = rowY(static_cast<int>(row));
        const int lane = m_layout.lanes[row];
        const int cx = laneCenterX(lane);
        const int cy = y + m_rowHeight / 2;
        const QColor color = laneColor(lane);
        const bool selected = static_cast<int>(row) == m_selectedRow;

        painter.setBrush(color);
        painter.setPen(selected ? QPen(palette().color(QPalette::HighlightedText), 2)
                              : QPen(Qt::NoPen));
        const int radius = m_commits[row].isMerge() ? 7 : 6;
        painter.drawEllipse(QPoint(cx, cy), radius, radius);
    }

    const QHash<int, RowGraphLabels> labelsByRow = collectRowLabels(m_layout);
    if (!labelsByRow.isEmpty()) {
        const int labelX = graphLanesWidth() + kLabelPadding;
        const QFontMetrics fm(font());

        for (auto it = labelsByRow.constBegin(); it != labelsByRow.constEnd(); ++it) {
            const int row = it.key();
            const RowGraphLabels &labels = it.value();
            const int y = rowY(row);
            const bool selected = row == m_selectedRow;
            int x = labelX;

            if (!labels.branchText.isEmpty()) {
                const QRect branchRect(x, y, std::max(0, width() - x), m_rowHeight);
                painter.setPen(selected ? palette().color(QPalette::HighlightedText)
                                        : palette().color(QPalette::Text));
                painter.drawText(branchRect,
                                 Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                                 labels.branchText);
                x += labelTextWidth(fm, labels.branchText);
                if (!labels.tagText.isEmpty()) {
                    x += labelTextWidth(fm, QStringLiteral("  "));
                }
            }

            if (!labels.tagText.isEmpty()) {
                const QRect tagRect(x, y, std::max(0, width() - x), m_rowHeight);
                drawTagLabel(painter, this, tagRect, labels.tagText, selected);
            }
        }
    }
}

void CommitGraphWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
        return;
    }

    const int row = rowAtY(static_cast<int>(event->position().y()));
    if (row < 0) {
        return;
    }

    setFocus(Qt::MouseFocusReason);
    emit rowClicked(row);
}

void CommitGraphWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    const int row = rowAtY(static_cast<int>(event->position().y()));
    if (row < 0) {
        return;
    }

    emit rowDoubleClicked(row);
}

void CommitGraphWidget::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_selectedRow >= 0) {
        emit rowDoubleClicked(m_selectedRow);
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}
