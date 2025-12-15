#include <gtest/gtest.h>
#include "../src/graph.hpp"
#include <string>

using namespace project_graph;

TEST(GraphProject, LoadValidCSV) {
    Graph g;
    std::string err;
    bool ok = Graph::from_csv("input/scv/example.csv", g, err);
    EXPECT_TRUE(ok);
    EXPECT_EQ(g.nodes().size(), 7);
}

TEST(GraphProject, NoCycleDetected) {
    Graph g;
    std::string err;
    ASSERT_TRUE(Graph::from_csv("input/scv/example.csv", g, err));
    EXPECT_FALSE(g.has_cycle());
}

TEST(GraphProject, TopologicalSortProducesOrder) {
    Graph g;
    std::string err;
    ASSERT_TRUE(Graph::from_csv("input/scv/example.csv", g, err));
    std::vector<std::string> order;
    ASSERT_TRUE(g.topological_sort(order));
    // basic check: A must come before B and C
    auto pos = [&](const std::string &id){
        for (size_t i=0;i<order.size();++i) if (order[i]==id) return (int)i;
        return -1;
    };
    EXPECT_LT(pos("A"), pos("B"));
    EXPECT_LT(pos("A"), pos("C"));
    EXPECT_LT(pos("B"), pos("D"));
    EXPECT_LT(pos("B"), pos("E"));
    EXPECT_LT(pos("C"), pos("E"));
}
