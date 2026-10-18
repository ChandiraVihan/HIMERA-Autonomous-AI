#include "himera/goap.hpp"
#include "himera/hollow.hpp"
#include "himera/dstar.hpp"
#include "himera/director.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
using namespace himera;
namespace {
int passed = 0, failed = 0;
void require(bool condition, const std::string& explanation) {
    if (!condition) throw std::runtime_error(explanation);
}
void test(const std::string& title, const std::function<void()>& execute) {
    try { execute(); ++passed; std::cout << "[PASS] " << title << '\n'; }
    catch (const std::exception& e) { ++failed; std::cerr << "[FAIL] " << title << " -> " << e.what() << '\n'; }
}
bool contains(const std::vector<std::string>& path, std::string step) {
    for (auto& p: path) if (p == step) return true;
    return false;
}
// Independent reference Dijkstra checks both dynamic optimal costs and no-path cases.
double referenceCost(const Grid& grid, Point start, Point goal) {
    const double INF = std::numeric_limits<double>::infinity();
    if (!grid.walkable(start) || !grid.walkable(goal)) return INF;
    std::vector<double> d(grid.count(), INF);
    using Item = std::pair<double, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> pq;
    d[grid.id(start)] = 0; pq.push({0, grid.id(start)});
    while (!pq.empty()) {
        auto [cost, id] = pq.top(); pq.pop();
        if (cost > d[id] + 1e-9) continue;
        if (grid.point(id) == goal) return cost;
        for (Point p : grid.neighbors(grid.point(id))) {
            int n = grid.id(p);
            double candidate = cost + 1.0 + grid.risk(p);
            if (candidate < d[n]) { d[n] = candidate; pq.push({candidate, n}); }
        }
    }
    return INF;
}
double validatePath(const Grid& g, const std::vector<Point>& path, Point start, Point goal) {
    require(!path.empty(), "Empty path");
    require(path.front() == start && path.back() == goal, "Wrong endpoints");
    double total = 0;
    for (size_t i = 0; i < path.size(); ++i) {
        require(g.walkable(path[i]), "Path crosses blocked cell");
        if (i) { require(manhattan(path[i-1], path[i]) == 1, "Non-adjacent route"); total += 1.0 + g.risk(path[i]); }
    }
    return total;
}
}
int main() {
    test("GOAP forms multi-step valid tactical plan", [] {
        SecurityGoapAgent s;
        Facts initial = bit(Fact::KnowsTarget) | bit(Fact::UnderFire);
        Plan p = s.plan(initial);
        require(p.achieved && p.steps.size() >= 4, "Expected cover, reload, aim and suppression");
        require(p.steps.front() == "TakeCover", "Unsafe plan: should seek cover first");
        require(contains(p.steps, "Reload") && contains(p.steps, "AcquireLineOfSight"), "Missing steps");
        for (auto& action : p.steps) initial = s.apply(initial, action);
        require(has(initial, Fact::TargetSuppressed), "Goal not achieved");
    });
    test("GOAP reacts to sound without known enemy", [] {
        SecurityGoapAgent s;
        Plan p = s.plan(bit(Fact::HeardSound));
        require(p.achieved && !p.steps.empty() && p.steps[0] == "InvestigateSound", "Wrong sound plan");
    });
    test("GOAP patrol fallback and invalid action", [] {
        SecurityGoapAgent s;
        require(s.plan(0).steps.front() == "PatrolSector", "Missing fallback");
        bool thrown = false;
        try { (void)s.apply(0, "SuppressTarget"); } catch(const std::logic_error&) { thrown = true; }
        require(thrown, "Invalid action accepted");
    });
    test("Hollow hearing increases belief near sound and keeps normalized distribution", [] {
        Grid g(8, 5); g.setBlocked({4, 2}, true);
        HollowBeliefAgent h(g);
        h.hear({6, 3}, 0.85, 0.99);
        require(h.probability({6, 3}) > h.probability({0, 0}), "Sound update not spatially meaningful");
        require(h.probability({4, 2}) == 0, "Blocked cell gained probability");
        require(std::abs(h.totalProbability() - 1) < 1e-9, "Belief mass not normalized");
        Point target = h.selectSearchCell({0,0});
        require(manhattan(target, {6, 3}) <= 1, "Wrong search location");
    });
    test("Hollow sight, diffusion and negative evidence", [] {
        HollowBeliefAgent h(Grid(5, 5));
        h.seePlayer({2,2});
        require(h.probability({2,2}) == 1, "Sight not certain");
        h.predict(0.8);
        require(h.probability({2,2}) < 1 && h.probability({2,1}) > 0, "Prediction did not diffuse");
        double before = h.probability({2,2});
        h.observeEmpty({{2,2}});
        require(h.probability({2,2}) < before, "Empty location did not reduce probability");
        require(std::abs(h.totalProbability() - 1) < 1e-9, "Lost probability mass");
    });
    test("Hollow group spread and invalid observation", [] {
        HollowBeliefAgent h(Grid(5, 5));
        h.hear({3,3}, 1.0, 0.99);
        Point first = h.selectSearchCell({0,0});
        Point second = h.selectSearchCell({0,0}, {first});
        require(first != second, "Allies selected same tile even when alternative exists");
        bool thrown = false;
        try { h.hear({0,0}, 0); } catch(const std::invalid_argument&) { thrown = true; }
        require(thrown, "Invalid sigma accepted");
    });
    test("D* Lite initial shortest path and no expansion on unchanged world", [] {
        Grid g(6,4);
        DStarLiteAgent d(g, {0,1}, {5,1});
        auto p = d.findPath();
        require(p.size() == 6, "Straight path length not 6 cells");
        require(std::abs(validatePath(d.grid(), p, {0,1}, {5,1}) - 5) < 1e-9, "Incorrect cost");
        auto again = d.findPath();
        require(again == p && d.lastExpansionCount() == 0, "Unchanged path unexpectedly recomputed");
    });
    test("D* Lite replans around newly locked door", [] {
        DStarLiteAgent d(Grid(6,4), {0,1}, {5,1});
        (void)d.findPath();
        d.updateCell({2,1}, true);
        auto path = d.findPath();
        require(std::abs(validatePath(d.grid(), path, {0,1}, {5,1}) - 7) < 1e-9, "Incorrect detour");
        for (auto p: path) require(p != Point{2,1}, "Route crosses new obstacle");
    });
    test("D* Lite responds to hazards with cheaper, safer path", [] {
        DStarLiteAgent d(Grid(5,3), {0,1}, {4,1});
        (void)d.findPath();
        d.updateCell({2,1}, false, 50.0);
        auto p = d.findPath();
        double actual = validatePath(d.grid(), p, {0,1}, {4,1});
        require(std::abs(actual - referenceCost(d.grid(), {0,1}, {4,1})) < 1e-9, "Suboptimal hazard route");
        for (Point point : p) require(point != Point{2,1}, "High-risk zone should be bypassed");
    });
    test("D* Lite correctly detects no path then reopened path", [] {
        DStarLiteAgent d(Grid(3,3), {0,1}, {2,1});
        (void)d.findPath();
        for (int y=0;y<3;++y) d.updateCell({1,y}, true);
        require(d.findPath().empty(), "Path incorrectly passes sealed wall");
        d.updateCell({1,0}, false);
        auto p = d.findPath();
        require(std::abs(validatePath(d.grid(), p, {0,1}, {2,1})-4) < 1e-9, "Reopened route unavailable");
    });
    test("D* Lite incremental moved start matches Dijkstra", [] {
        DStarLiteAgent d(Grid(7,4), {0,0}, {6,3});
        (void)d.findPath();
        d.moveStart({1,0}); d.updateCell({3,1}, true); d.updateCell({5,2}, false, 4);
        auto p = d.findPath();
        double actual = validatePath(d.grid(), p, {1,0}, {6,3});
        require(std::abs(actual - referenceCost(d.grid(), {1,0}, {6,3})) < 1e-9, "Wrong moved-start cost");
    });
    test("D* Lite many map changes agree with independent Dijkstra", [] {
        DStarLiteAgent d(Grid(9,7), {0,0}, {8,6});
        for (int step=0; step < 30; ++step) {
            Point p{1 + (step*5)%7, (step*3)%7};
            d.updateCell(p, step%3 == 0, (step%3 == 0 ? 0 : (step%5)));
            auto path = d.findPath();
            double expected = referenceCost(d.grid(), {0,0}, {8,6});
            if (std::isinf(expected)) require(path.empty(), "Expected disconnected map");
            else require(!path.empty() && std::abs(validatePath(d.grid(), path, {0,0}, {8,6}) - expected)<1e-8,
                         "D* Lite differs from Dijkstra at change " + std::to_string(step));
        }
    });
    test("Director selects Hollow wave at high exposure with adequate resources", [] {
        ExposureDirectorAgent d;
        auto result = d.choose({.health01=.9,.ammo=25,.exposure01=.85,.stress01=.5,
                                .activeHollows=0,.secondsSinceWave=60,.safeZone=false});
        require(result.chosen == Encounter::HollowWave, "High-exposure scenario not using wave");
    });
    test("Director refuses Hollow wave when health is low or cooldown active", [] {
        ExposureDirectorAgent d;
        auto low = d.choose({.health01=.2,.ammo=25,.exposure01=1,.stress01=.8,.activeHollows=0,.secondsSinceWave=90});
        auto cooldown = d.choose({.health01=.9,.ammo=25,.exposure01=1,.stress01=.8,.activeHollows=0,.secondsSinceWave=4});
        require(low.chosen != Encounter::HollowWave && cooldown.chosen != Encounter::HollowWave,
                "Safety constraints not applied");
    });
    test("Director respects safe zones and enemy count cap", [] {
        ExposureDirectorAgent d;
        auto safe = d.choose({.health01=1,.ammo=30,.exposure01=1,.stress01=1,.activeHollows=0,.secondsSinceWave=90,.safeZone=true});
        auto cap = d.choose({.health01=1,.ammo=30,.exposure01=1,.stress01=1,.activeHollows=4,.secondsSinceWave=90});
        require(safe.chosen != Encounter::HollowWave && cap.chosen != Encounter::HollowWave,
                "Director ignored spawn fairness");
    });
    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
