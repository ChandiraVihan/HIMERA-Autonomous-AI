#include "himera/dstar.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <vector>
using namespace himera;
namespace {
struct SearchResult { double cost; int expansions; };
SearchResult freshAStar(const Grid& g, Point start, Point goal) {
    constexpr double INF = std::numeric_limits<double>::infinity();
    if (!g.walkable(start) || !g.walkable(goal)) return {INF, 0};
    struct Entry {double priority; double cost; int id; bool operator>(const Entry& b) const { return priority > b.priority; }};
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
    std::vector<double> distances(g.count(), INF);
    distances[g.id(start)] = 0;
    open.push({double(manhattan(start,goal)), 0, g.id(start)});
    int expansions = 0;
    while (!open.empty()) {
        auto current = open.top(); open.pop();
        if (current.cost > distances[current.id] + 1e-9) continue;
        ++expansions;
        if (g.point(current.id) == goal) return {current.cost, expansions};
        for (Point p : g.neighbors(g.point(current.id))) {
            int next = g.id(p);
            double cost = current.cost + 1 + g.risk(p);
            if (cost + 1e-9 < distances[next]) {
                distances[next] = cost;
                open.push({cost + manhattan(p, goal), cost, next});
            }
        }
    }
    return {INF, expansions};
}
double pathCost(const Grid& g, const std::vector<Point>& path) {
    if (path.empty()) return std::numeric_limits<double>::infinity();
    double total=0;
    for (size_t i=1; i<path.size(); ++i) total += 1 + g.risk(path[i]);
    return total;
}
}
int main() {
    Grid grid(40, 20);
    Point start{1, 9}, goal{38, 9};
    DStarLiteAgent incremental(grid, start, goal);
    std::int64_t dstarUs=0, astarUs=0;
    int dstarExpanded=0, astarExpanded=0;
    bool verified=true;
    std::cout << "HIMERA navigation algorithm comparison (same changing grid)\n";
    std::cout << "Step | D* Lite expanded | fresh A* expanded | cost\n";
    for(int step=0; step<25; ++step) {
        Point changed{7+(step*7)%27, 7+(step*3)%5};
        // Alternate door obstruction and traversal hazard in a narrow belt.
        bool blocked = (step%4==0);
        double risk = blocked ? 0 : ((step%4==1) ? 12.0 : 0.0);
        incremental.updateCell(changed, blocked, risk);
        grid.setBlocked(changed, blocked);
        grid.setRisk(changed, risk);
        auto before=std::chrono::steady_clock::now();
        auto route = incremental.findPath();
        auto after=std::chrono::steady_clock::now();
        auto fresh = freshAStar(grid, start, goal);
        auto finish=std::chrono::steady_clock::now();
        dstarUs += std::chrono::duration_cast<std::chrono::microseconds>(after-before).count();
        astarUs += std::chrono::duration_cast<std::chrono::microseconds>(finish-after).count();
        int expandedDstar = incremental.lastExpansionCount();
        dstarExpanded += expandedDstar;
        astarExpanded += fresh.expansions;
        double cost = pathCost(grid, route);
        if (std::isinf(cost) != std::isinf(fresh.cost) ||
            (!std::isinf(cost) && std::abs(cost - fresh.cost)>1e-8)) verified=false;
        std::cout << std::setw(4) << step << " | " << std::setw(16) << expandedDstar
                  << " | " << std::setw(17) << fresh.expansions << " | " << cost << '\n';
    }
    std::cout << "\nTotal expansions: D* Lite=" << dstarExpanded << ", A*=" << astarExpanded << '\n';
    std::cout << "Observed times (microseconds, environment-specific): D* Lite=" << dstarUs
              << ", A*=" << astarUs << '\n';
    std::cout << "Path costs verified against A*: " << (verified ? "YES" : "NO") << '\n';
    std::cout << "Warning: one small synthetic trial is not an Unreal performance benchmark.\n";
    return verified ? 0 : 1;
}
