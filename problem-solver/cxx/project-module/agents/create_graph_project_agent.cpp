#include <iostream>
#include "../src/graph.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: create_graph_project_agent <input.csv>\n";
        return 2;
    }
    std::string path = argv[1];
    project_graph::Graph g;
    std::string err;
    if (!project_graph::Graph::from_csv(path, g, err)) {
        std::cerr << "Error building graph: " << err << "\n";
        return 1;
    }
    std::cout << "Graph built. Nodes: " << g.nodes().size() << "\n";
    return 0;
}
