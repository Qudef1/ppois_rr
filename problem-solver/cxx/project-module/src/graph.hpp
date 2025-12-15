#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace project_graph {

struct Node {
    std::string id;
    std::string name;
    int duration = 0;
    std::vector<std::string> deps; // dependencies -> edges from deps to this
};

class Graph {
public:
    // load from csv file: id,name,duration,dependencies(semicolon separated ids)
    static bool from_csv(const std::string &path, Graph &out, std::string &err);

    // detect if graph has cycle
    bool has_cycle() const;

    // produce topological order; returns false if cycle detected
    bool topological_sort(std::vector<std::string> &order) const;

    const std::unordered_map<std::string, Node>& nodes() const { return nodes_; }

private:
    std::unordered_map<std::string, Node> nodes_;
    bool build_internal(std::string &err);
    friend class GraphBuilder;
};

}
