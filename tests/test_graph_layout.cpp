#include "core/GraphLayout.h"

#include <QtTest>

class TestGraphLayout : public QObject {
    Q_OBJECT

private slots:
    void assignsLanesForLinearHistory();
    void mergeCommitUsesMultipleLanes();
};

void TestGraphLayout::assignsLanesForLinearHistory()
{
    std::vector<Commit> commits(2);
    commits[0].hash = QStringLiteral("c2");
    commits[0].parentHashes = {QStringLiteral("c1")};
    commits[0].subject = QStringLiteral("Second");
    commits[1].hash = QStringLiteral("c1");
    commits[1].parentHashes = {};
    commits[1].subject = QStringLiteral("First");

    const GraphLayout layout = GraphLayout::build(commits);
    QCOMPARE(layout.lanes.size(), size_t(2));
    QCOMPARE(layout.laneCount, 1);
    QCOMPARE(layout.lanes[0], layout.lanes[1]);
    QCOMPARE(layout.edges.size(), size_t(1));
}

void TestGraphLayout::mergeCommitUsesMultipleLanes()
{
    std::vector<Commit> commits(4);
    commits[0].hash = QStringLiteral("merge");
    commits[0].parentHashes = {QStringLiteral("main"), QStringLiteral("feature")};

    commits[1].hash = QStringLiteral("main");
    commits[1].parentHashes = {QStringLiteral("root")};

    commits[2].hash = QStringLiteral("feature");
    commits[2].parentHashes = {QStringLiteral("root")};

    commits[3].hash = QStringLiteral("root");
    commits[3].parentHashes = {};

    const GraphLayout layout = GraphLayout::build(commits);
    QVERIFY(layout.laneCount >= 2);
    QCOMPARE(layout.edges.size(), size_t(4));
}

QTEST_MAIN(TestGraphLayout)
#include "test_graph_layout.moc"
