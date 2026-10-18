#include "himera/goap.hpp"
#include "himera/hollow.hpp"
#include "himera/dstar.hpp"
#include "himera/director.hpp"
#include <iomanip>
#include <iostream>
using namespace himera;
int main() {
    std::cout << "HIMERA | Autonomous AI standalone demonstration\n\n";
    SecurityGoapAgent security;
    Facts initial = bit(Fact::KnowsTarget) | bit(Fact::UnderFire);
    Plan plan = security.plan(initial);
    std::cout << "[1] SECURITY GOAP  | expanded " << plan.expandedStates << " planning states\n";
    for (const auto& step : plan.steps) std::cout << "    -> " << step << '\n';
    std::cout << "    Total symbolic plan cost: " << plan.cost << "\n\n";

    Grid map(7, 5);
    map.setBlocked({3, 1}, true); map.setBlocked({3, 2}, true);
    HollowBeliefAgent hollow(map);
    hollow.hear({5, 3}, 1.0, 0.99);
    Point suspected = hollow.selectSearchCell({1, 1});
    std::cout << "[2] HOLLOW BELIEF  | gunshot detected near (5,3)\n";
    std::cout << "    Search target: (" << suspected.x << ',' << suspected.y << ")\n";
    std::cout << "    Probability mass: " << std::setprecision(6) << hollow.totalProbability() << "\n\n";

    Grid facility(6, 4);
    DStarLiteAgent trooper(facility, {0, 1}, {5, 1});
    auto before = trooper.findPath();
    int beforeExpansions = trooper.lastExpansionCount();
    trooper.updateCell({2, 1}, true);
    auto after = trooper.findPath();
    std::cout << "[3] TROOPER D* LITE  | locked door at (2,1)\n";
    std::cout << "    Initial path: " << before.size() << " cells, " << beforeExpansions << " expansions\n";
    std::cout << "    Replanned path: " << after.size() << " cells, " << trooper.lastExpansionCount() << " expansions\n";
    for (Point p : after) std::cout << "    (" << p.x << ',' << p.y << ")";
    std::cout << "\n\n";

    ExposureDirectorAgent director;
    auto decision = director.choose({.health01=0.9, .ammo=25, .exposure01=0.85,
                                     .stress01=0.5, .activeHollows=0, .secondsSinceWave=60, .safeZone=false});
    std::cout << "[4] EXPOSURE DIRECTOR  | " << toString(decision.chosen) << "\n";
    for (const auto& s : decision.scores)
        std::cout << "    " << toString(s.encounter) << ": utility=" << s.utility
                  << ", allowed=" << std::boolalpha << s.allowed << "\n";
    return 0;
}
