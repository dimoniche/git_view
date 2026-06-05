#include "ui/CommitHistoryView.h"

#include "ui/CommitGraphWidget.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

CommitHistoryView::CommitHistoryView(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(m_splitter);

    m_graphColumn = new QWidget(m_splitter);
    auto *graphColumnLayout = new QVBoxLayout(m_graphColumn);
    graphColumnLayout->setContentsMargins(0, 0, 0, 0);
    graphColumnLayout->setSpacing(0);

    m_graphHeader = new QWidget(m_graphColumn);
    m_graphHeader->setFixedHeight(kDefaultHeaderHeight);
    m_graphHeader->setAutoFillBackground(true);
    m_graphHeader->setBackgroundRole(QPalette::Midlight);
    graphColumnLayout->addWidget(m_graphHeader);

    m_graphScroll = new QScrollArea(m_graphColumn);
    m_graphScroll->setWidgetResizable(false);
    m_graphScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_graphScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphScroll->setFrameShape(QFrame::NoFrame);
    m_graphScroll->setFocusPolicy(Qt::NoFocus);
    graphColumnLayout->addWidget(m_graphScroll, 1);

    m_graph = new CommitGraphWidget;
    m_graphScroll->setWidget(m_graph);
    m_splitter->addWidget(m_graphColumn);

    m_table = new QTableView(m_splitter);
    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels(
        {tr("Hash"), tr("Author"), tr("Date"), tr("Subject")});
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->setDefaultSectionSize(kRowHeight);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setMinimumSectionSize(40);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setColumnWidth(0, 80);
    m_table->setColumnWidth(1, 140);
    m_table->setColumnWidth(2, 170);
    m_splitter->addWidget(m_table);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({220, 700});

    connect(m_graph, &CommitGraphWidget::rowClicked, this,
            [this](int row) { selectRow(row, true); });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &current, const QModelIndex &previous) {
                Q_UNUSED(previous);
                if (m_syncingSelection || !current.isValid()) {
                    return;
                }
                selectRow(current.row(), false);
            });
    connect(m_table, &QTableView::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                const QModelIndex index = m_table->indexAt(pos);
                if (index.isValid()) {
                    showCommitContextMenu(index.row(), m_table->viewport()->mapToGlobal(pos));
                }
            });

    connect(m_table->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { syncGraphScrollFromTable(); });
    connect(m_graphScroll->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { syncTableScrollFromGraph(); });

    m_table->horizontalHeader()->installEventFilter(this);
    m_table->verticalHeader()->installEventFilter(this);
    m_graphScroll->viewport()->installEventFilter(this);
}

bool CommitHistoryView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_graphScroll->viewport() && event->type() == QEvent::Wheel) {
        QApplication::sendEvent(m_table->viewport(), event);
        return true;
    }

    if ((watched == m_table->horizontalHeader() || watched == m_table->verticalHeader())
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateGraphGeometry();
    }
    return QWidget::eventFilter(watched, event);
}

void CommitHistoryView::setCommits(const std::vector<Commit> &commits,
                                   const QString &branchTipHash,
                                   const std::vector<Branch> &branches)
{
    m_commits = commits;
    m_layout = GraphLayout::build(m_commits, branchTipHash, branches);
    m_graph->setData(m_commits, m_layout);

    m_model->removeRows(0, m_model->rowCount());
    for (const Commit &commit : m_commits) {
        QList<QStandardItem *> row;
        auto *hashItem = new QStandardItem(commit.hash.left(8));
        auto *authorItem = new QStandardItem(commit.author);
        auto *dateItem = new QStandardItem(commit.date);
        auto *subjectItem = new QStandardItem(commit.subject);
        for (QStandardItem *item : {hashItem, authorItem, dateItem, subjectItem}) {
            item->setEditable(false);
        }
        row << hashItem << authorItem << dateItem << subjectItem;
        m_model->appendRow(row);
    }

    updateGraphGeometry();

    if (m_commits.empty()) {
        m_selectedRow = -1;
        return;
    }

    selectRow(0, false);
}

QString CommitHistoryView::selectedHash() const
{
    if (m_selectedRow < 0 || m_selectedRow >= static_cast<int>(m_commits.size())) {
        return {};
    }
    return m_commits[static_cast<size_t>(m_selectedRow)].hash;
}

void CommitHistoryView::updateGraphGeometry()
{
    int headerHeight = m_table->horizontalHeader()->height();
    if (headerHeight <= 0) {
        headerHeight = kDefaultHeaderHeight;
    }

    int rowHeight = m_table->verticalHeader()->sectionSize(0);
    if (rowHeight <= 0) {
        rowHeight = kRowHeight;
    }
    m_table->verticalHeader()->setDefaultSectionSize(rowHeight);
    m_graph->setRowHeight(rowHeight);

    const int rows = static_cast<int>(m_commits.size());
    const int height = rows * rowHeight;
    const int width = m_graph->naturalWidth();

    m_graphHeader->setFixedHeight(headerHeight);
    m_graph->setFixedSize(std::max(width, 1), std::max(height, 1));

    QScrollBar *tableBar = m_table->verticalScrollBar();
    QScrollBar *graphBar = m_graphScroll->verticalScrollBar();
    graphBar->setRange(tableBar->minimum(), tableBar->maximum());
    graphBar->setPageStep(tableBar->pageStep());
    graphBar->setSingleStep(tableBar->singleStep());

    syncGraphScrollFromTable();
}

void CommitHistoryView::syncGraphScrollFromTable()
{
    if (m_syncingScroll) {
        return;
    }

    m_syncingScroll = true;
    m_graphScroll->verticalScrollBar()->setValue(m_table->verticalScrollBar()->value());
    m_syncingScroll = false;
}

void CommitHistoryView::syncTableScrollFromGraph()
{
    if (m_syncingScroll) {
        return;
    }

    m_syncingScroll = true;
    m_table->verticalScrollBar()->setValue(m_graphScroll->verticalScrollBar()->value());
    m_syncingScroll = false;
}

void CommitHistoryView::selectRow(int row, bool scrollIntoView)
{
    if (row < 0 || row >= static_cast<int>(m_commits.size())) {
        return;
    }

    m_syncingSelection = true;

    if (row != m_selectedRow) {
        m_selectedRow = row;
        m_graph->setSelectedRow(row);
        emit commitSelected(m_commits[static_cast<size_t>(row)].hash);
    } else {
        m_graph->setSelectedRow(row);
    }

    const QModelIndex index = m_model->index(row, 0);
    if (index.isValid()) {
        m_table->setCurrentIndex(index);
        if (scrollIntoView) {
            m_table->scrollTo(index, QAbstractItemView::PositionAtCenter);
            syncGraphScrollFromTable();
        }
    }

    m_syncingSelection = false;
}

void CommitHistoryView::showCommitContextMenu(int row, const QPoint &globalPos)
{
    if (row < 0 || row >= static_cast<int>(m_commits.size())) {
        return;
    }

    selectRow(row, true);
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
