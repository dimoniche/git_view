#include "core/GraphLayout.h"
#include "core/Tag.h"

#include <QtTest>

class TestGraphLayout : public QObject {
    Q_OBJECT

private slots:
    void assignsLanesForLinearHistory();
    void mergeCommitUsesMultipleLanes();
    void branchTrunkIsStraightLine();
    void reusesSideLaneWhenBranchesDoNotOverlap();
    void showsTagsOnTaggedCommits();
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

void TestGraphLayout::branchTrunkIsStraightLine()
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

    const GraphLayout layout = GraphLayout::build(commits, QStringLiteral("merge"));
    QCOMPARE(layout.lanes[0], layout.lanes[1]);
    QCOMPARE(layout.lanes[0], layout.lanes[3]);
    QVERIFY(layout.lanes[2] > layout.lanes[0]);
    QCOMPARE(layout.edges.size(), size_t(4));
}

void TestGraphLayout::reusesSideLaneWhenBranchesDoNotOverlap()
{
    std::vector<Commit> commits(8);
    commits[0].hash = QStringLiteral("merge_new");
    commits[0].parentHashes = {QStringLiteral("trunk_mid"), QStringLiteral("side_new")};

    commits[1].hash = QStringLiteral("trunk_mid");
    commits[1].parentHashes = {QStringLiteral("trunk_old")};

    commits[2].hash = QStringLiteral("trunk_old");
    commits[2].parentHashes = {QStringLiteral("merge_old")};

    commits[3].hash = QStringLiteral("side_new");
    commits[3].parentHashes = {QStringLiteral("root")};

    commits[4].hash = QStringLiteral("merge_old");
    commits[4].parentHashes = {QStringLiteral("trunk_base"), QStringLiteral("side_old")};

    commits[5].hash = QStringLiteral("side_old");
    commits[5].parentHashes = {QStringLiteral("root")};

    commits[6].hash = QStringLiteral("trunk_base");
    commits[6].parentHashes = {QStringLiteral("root")};

    commits[7].hash = QStringLiteral("root");
    commits[7].parentHashes = {};

    const GraphLayout layout = GraphLayout::build(commits, QStringLiteral("merge_new"));
    QCOMPARE(layout.lanes[3], layout.lanes[5]);
    QCOMPARE(layout.laneCount, 2);
}

void TestGraphLayout::showsTagsOnTaggedCommits()
{
    std::vector<Commit> commits(2);
    commits[0].hash = QStringLiteral("c2");
    commits[0].parentHashes = {QStringLiteral("c1")};
    commits[1].hash = QStringLiteral("c1");
    commits[1].parentHashes = {};

    std::vector<Tag> tags(2);
    tags[0].name = QStringLiteral("v2.0");
    tags[0].tipHash = QStringLiteral("c2");
    tags[1].name = QStringLiteral("v1.0");
    tags[1].tipHash = QStringLiteral("c1");

    const GraphLayout layout = GraphLayout::build(commits, {}, {}, tags);
    QCOMPARE(layout.rowLabels.size(), size_t(2));
    QCOMPARE(layout.rowLabels[0].row, 0);
    QCOMPARE(layout.rowLabels[0].name, QStringLiteral("v2.0"));
    QCOMPARE(layout.rowLabels[1].row, 1);
    QCOMPARE(layout.rowLabels[1].name, QStringLiteral("v1.0"));
}

QTEST_MAIN(TestGraphLayout)
#include "test_graph_layout.moc"
