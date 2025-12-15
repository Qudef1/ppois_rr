#include "graph.hpp"
#include <fstream>
#include <sstream>
#include <queue>
#include <set>

using namespace project_graph;

static std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream iss(s);
    while (std::getline(iss, cur, sep)) {
        // trim
        size_t a = cur.find_first_not_of(" \t\r\n");
        size_t b = cur.find_last_not_of(" \t\r\n");
        if (a==std::string::npos) continue;
        out.push_back(cur.substr(a, b-a+1));
    }
    return out;
}

bool Graph::from_csv(const std::string &path, Graph &out, std::string &err) {
    std::ifstream ifs(path);
    if (!ifs) { err = "Cannot open file: " + path; return false; }

    std::string line;
    bool first = true;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        if (first) { first = false; continue; } // skip header
        auto cols = split(line, ',');
        if (cols.size() < 3) { err = "Invalid CSV row: " + line; return false; }
        Node n;
        n.id = cols[0];
        n.name = cols[1];
        try { n.duration = std::stoi(cols[2]); } catch(...) { n.duration = 0; }
        if (cols.size() >= 4 && !cols[3].empty()) {
            auto deps = split(cols[3], ';');
            n.deps = deps;
        }
        out.nodes_.emplace(n.id, std::move(n));
    }
    return out.build_internal(err);
}

bool Graph::build_internal(std::string &err) {
    // verify dependencies refer to known nodes (it's okay if not present; we'll accept but warn)
    for (auto &p : nodes_) {
        for (auto &d : p.second.deps) {
            if (nodes_.find(d) == nodes_.end()) {
                // allow missing dependency but add a placeholder node
                Node ph; ph.id = d; ph.name = d; ph.duration = 0;
                nodes_.emplace(d, std::move(ph));
            }
        }
    }
    return true;
}

bool Graph::has_cycle() const {
    // DFS with coloring
    enum Color { WHITE=0, GRAY=1, BLACK=2 };
    std::unordered_map<std::string, int> color;
    for (auto &p: nodes_) color[p.first] = WHITE;

    std::function<bool(const std::string&)> dfs = [&](const std::string &u)->bool{
        color[u] = GRAY;
        auto it = nodes_.find(u);
        if (it!=nodes_.end()){
            for (auto &v: it->second.deps) {
                if (color[v] == GRAY) return true;
                if (color[v] == WHITE) {
                    if (dfs(v)) return true;
                }
            }
        }
        color[u] = BLACK;
        return false;
    };

    for (auto &p: nodes_) {
        if (color[p.first] == WHITE) {
            if (dfs(p.first)) return true;
        }
    }
    return false;
}

bool Graph::topological_sort(std::vector<std::string> &order) const {
    // Kahn's algorithm
    std::unordered_map<std::string, int> indeg;
    for (auto &p: nodes_) indeg[p.first] = 0;
    for (auto &p: nodes_) {
        for (auto &d: p.second.deps) {
            indeg[p.first]++; // edge from d -> p
        }
    }
    std::queue<std::string> q;
    for (auto &p: indeg) if (p.second==0) q.push(p.first);

    order.clear();
    while (!q.empty()) {
        auto u = q.front(); q.pop();
        order.push_back(u);
        // remove edges u->v where v has dependency on u; in our model edges stored reversed, so we must check nodes whose deps include u
        for (auto &p: nodes_) {
            // if p depends on u
            bool found=false;
            for (auto &d: p.second.deps) if (d==u) { found=true; break; }
            if (found) {
                indeg[p.first]--;
                if (indeg[p.first]==0) q.push(p.first);
            }
        }
    }
    if (order.size() != nodes_.size()) return false; // cycle or disconnected placeholder
    return true;
}
