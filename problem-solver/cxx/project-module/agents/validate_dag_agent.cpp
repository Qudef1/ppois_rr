#include <iostream>
#include "../src/graph.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: validate_dag_agent <input.csv>\n";
        return 2;
    }
    std::string path = argv[1];
    project_graph::Graph g;
    std::string err;
    if (!project_graph::Graph::from_csv(path, g, err)) {
        std::cerr << "Error building graph: " << err << "\n";
        return 1;
    }
    bool cyc = g.has_cycle();
    if (cyc) {
        std::cout << "Plan is invalid: cycle detected\n";
        return 3;
    }
    std::vector<std::string> order;
    if (!g.topological_sort(order)) {
        std::cout << "Plan is invalid: cannot produce topological order\n";
        return 4;
    }
    std::cout << "Plan is valid. Topological order:\n";
    for (auto &id : order) std::cout << id << " ";
    std::cout << "\n";
    return 0;
}
