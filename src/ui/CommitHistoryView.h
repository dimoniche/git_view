#pragma once

#include "core/Branch.h"
#include "core/Commit.h"
#include "core/GraphLayout.h"

#include <QWidget>
#include <vector>

class CommitGraphWidget;
class QScrollArea;
class QSplitter;
class QStandardItemModel;
class QTableView;

class CommitHistoryView : public QWidget {
    Q_OBJECT

public:
    explicit CommitHistoryView(QWidget *parent = nullptr);

    void setCommits(const std::vector<Commit> &commits,
                    const QString &branchTipHash = {},
                    const std::vector<Branch> &branches = {});
    int selectedRow() const { return m_selectedRow; }
    QString selectedHash() const;

signals:
    void commitSelected(const QString &hash);
    void viewCommitDetailsRequested(const QString &hash);
    void createBranchFromCommitRequested(const QString &hash);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void selectRow(int row, bool scrollIntoView = true);
    void openCommitDetails(int row);
    void showCommitContextMenu(int row, const QPoint &globalPos);
    void updateGraphGeometry();
    void syncGraphScrollFromTable();
    void syncTableScrollFromGraph();

    QSplitter *m_splitter = nullptr;
    QWidget *m_graphColumn = nullptr;
    QWidget *m_graphHeader = nullptr;
    QScrollArea *m_graphScroll = nullptr;
    CommitGraphWidget *m_graph = nullptr;
    QTableView *m_table = nullptr;
    QStandardItemModel *m_model = nullptr;

    std::vector<Commit> m_commits;
    GraphLayout m_layout;
    int m_selectedRow = -1;
    bool m_syncingScroll = false;
    bool m_syncingSelection = false;

    static constexpr int kRowHeight = 34;
    static constexpr int kDefaultHeaderHeight = 30;
};
