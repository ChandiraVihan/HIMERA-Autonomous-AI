// HIMERA Visual Simulator — a small scenario adapter around the existing C++
// agent library. Protocol: one ASCII command per line in, one JSON snapshot out.
// No agent decision algorithm is reimplemented in the UI or in this adapter.
#include "himera/goap.hpp"
#include "himera/hollow.hpp"
#include "himera/dstar.hpp"
#include "himera/director.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace himera;
namespace {
constexpr int W = 20;
constexpr int H = 12;
constexpr Point DOOR{9, 5};

std::string quote(const std::string& s) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else if (c == '\t') out << "\\t";
        else if (c < 32) out << '?';
        else out << c;
    }
    out << '"';
    return out.str();
}
std::string pt(Point p) { return "[" + std::to_string(p.x) + "," + std::to_string(p.y) + "]"; }
std::vector<Point> route(const Grid& g, Point from, Point to) {
    if (!g.walkable(from) || !g.walkable(to)) return {};
    std::vector<int> parent(g.count(), -1);
    std::queue<Point> q;
    q.push(from); parent[g.id(from)] = g.id(from);
    while (!q.empty()) {
        Point at = q.front(); q.pop();
        if (at == to) break;
        for (Point n : g.neighbors(at)) {
            if (parent[g.id(n)] >= 0) continue;
            parent[g.id(n)] = g.id(at); q.push(n);
        }
    }
    if (parent[g.id(to)] == -1) return {};
    std::vector<Point> result;
    for (Point p = to;; p = g.point(parent[g.id(p)])) {
        result.push_back(p);
        if (p == from) break;
    }
    std::reverse(result.begin(), result.end());
    return result;
}
std::string errorJson(const std::string& msg) {
    return "{\"ok\":false,\"error\":" + quote(msg) + "}";
}

class Simulation {
    Grid world_{W,H};
    SecurityGoapAgent securityBrain_;
    ExposureDirectorAgent directorBrain_;
    std::unique_ptr<HollowBeliefAgent> hollowBrain_;
    std::unique_ptr<DStarLiteAgent> trooperBrain_;
    Point player_{16,7}, security_{3,2}, hollow_{3,9}, trooper_{5,3};
    Facts securityFacts_ = 0;
    bool doorOpen_ = true, safeZone_ = false;
    int tick_ = 0, ammo_ = 18, activeHollows_ = 0;
    double health_ = 0.85, exposure_ = 0.68, stress_ = 0.48, secondsSinceWave_ = 80;
    std::string lastEvent_ = "Scenario loaded. Choose an action to see the agents respond.";
    std::deque<std::string> history_;
    std::vector<Point> trooperPath_;
    int expanded_ = 0;
    Point hollowTarget_{3,9};
    std::string securityLast_ = "Idle";
    Encounter directorLast_ = Encounter::Quiet;

    void event(const std::string& s) {
        lastEvent_ = s;
        history_.push_front("T" + std::to_string(tick_) + " · " + s);
        if (history_.size() > 12) history_.pop_back();
    }
    void refreshPath() {
        trooperPath_ = trooperBrain_->findPath();
        expanded_ = trooperBrain_->lastExpansionCount();
    }
    void build() {
        world_ = Grid(W,H);
        for (int y=0;y<H;++y) for (int x=0;x<W;++x) {
            if (x == 0 || y == 0 || x == W-1 || y == H-1) world_.setBlocked({x,y},true);
        }
        // Central wall: one controllable door and one long detour (at y=9).
        for (int y=1;y<=10;++y) if (y != 5 && y != 9) world_.setBlocked({9,y},true);
        // Cover, chemical tanks and storage crates, shaped to leave both passages traversable.
        for (Point p : {Point{5,4},Point{5,5},Point{6,5},Point{6,8},Point{12,3},Point{12,4},
                        Point{13,4},Point{14,6},Point{15,6},Point{3,5}}) world_.setBlocked(p,true);
        doorOpen_ = true;
        player_ = {16,7}; security_ = {3,2}; hollow_={3,9}; trooper_={5,3};
        tick_=0; ammo_=18; activeHollows_=0;
        health_=0.85; exposure_=0.68; stress_=0.48; secondsSinceWave_=80;
        safeZone_=false; securityFacts_=0;
        hollowBrain_ = std::make_unique<HollowBeliefAgent>(world_);
        trooperBrain_ = std::make_unique<DStarLiteAgent>(world_,trooper_,player_);
        hollowTarget_=hollow_;
        securityLast_="Idle";
        directorLast_=Encounter::Quiet;
        refreshPath();
        history_.clear();
        event("Simulation ready. All four algorithms are running in C++.");
    }
    DirectorState directorState() const {
        return {.health01=health_,.ammo=ammo_,.exposure01=exposure_,.stress01=stress_,
                .activeHollows=activeHollows_,.secondsSinceWave=secondsSinceWave_,.safeZone=safeZone_};
    }
    void step() {
        ++tick_;
        secondsSinceWave_ += 5;
        hollowBrain_->predict(0.23);
        if (manhattan(hollow_,player_)<=2) hollowBrain_->seePlayer(player_);
        hollowTarget_ = hollowBrain_->selectSearchCell(hollow_);
        auto hp = route(world_, hollow_, hollowTarget_);
        if (hp.size() > 1) hollow_=hp[1];
        if (hollow_ != player_) hollowBrain_->observeEmpty({hollow_},0.08);

        Plan p = securityBrain_.plan(securityFacts_);
        if (p.achieved && !p.steps.empty()) {
            securityLast_=p.steps.front();
            securityFacts_ = securityBrain_.apply(securityFacts_, securityLast_);
        } else securityLast_ = "Observe";
        Point secTarget = has(securityFacts_,Fact::KnowsTarget)
            ? (has(securityFacts_,Fact::InCover) ? Point{7,6} : player_)
            : Point{7,2};
        auto sp = route(world_,security_,secTarget);
        if (sp.size()>1 && securityLast_!="SuppressTarget") security_=sp[1];

        // D* Lite follows its own optimal route. The plan is kept incrementally
        // when the door closes or opens; a changed GOAL needs a fresh planner.
        refreshPath();
        if (trooperPath_.size()>1) {
            Point next=trooperPath_[1];
            trooperBrain_->moveStart(next);
            trooper_=next;
            refreshPath();
        }

        auto choice = directorBrain_.choose(directorState());
        directorLast_=choice.chosen;
        if (choice.chosen == Encounter::HollowWave) {
            ++activeHollows_;
            secondsSinceWave_=0;
            event("HIM-7 Director triggered HollowWave; cooldown now active.");
        } else event("Simulation advanced 5 s. Director selected " + toString(choice.chosen) + ".");
    }
    static void paths(std::ostringstream& out,const std::vector<Point>& list) {
        out << '[';
        for(size_t i=0;i<list.size();++i) {if(i)out<<',';out<<pt(list[i]);}
        out << ']';
    }
public:
    Simulation(){build();}
    std::string handle(const std::string& command) {
        try {
            std::istringstream in(command);
            std::string name; in >> name;
            if (name=="RESET") build();
            else if (name=="STEP") step();
            else if (name=="GUNSHOT") {
                hollowBrain_->hear(player_,1.8,0.94);
                hollowTarget_ = hollowBrain_->selectSearchCell(hollow_);
                securityFacts_ |= bit(Fact::HeardSound);
                stress_=std::min(1.0,stress_+0.12);
                ammo_=std::max(0,ammo_-1);
                event("Gunshot: Hollow belief updated; Security hears an anomaly.");
            } else if (name=="SPOT") {
                securityFacts_ |= bit(Fact::KnowsTarget)|bit(Fact::UnderFire);
                event("Security spotted the player. GOAP now plans a tactical response.");
            } else if (name=="DOOR") {
                doorOpen_=!doorOpen_;
                world_.setBlocked(DOOR,!doorOpen_);
                trooperBrain_->updateCell(DOOR,!doorOpen_);
                // Hollow's belief grid is immutable in v0.1; rebuild and preserve
                // recent evidence only when changing the map topology.
                hollowBrain_=std::make_unique<HollowBeliefAgent>(world_);
                hollowTarget_=hollowBrain_->selectSearchCell(hollow_);
                refreshPath();
                event(std::string("Containment door ")+(doorOpen_?"opened":"closed")+
                      ". D* Lite repaired the existing search graph; Hollow belief reset for new topology.");
            } else if (name=="PLAYER") {
                int x,y; if(!(in>>x>>y)) throw std::invalid_argument("Expected PLAYER x y");
                Point p{x,y}; if(!world_.walkable(p)) throw std::invalid_argument("Choose a walkable floor tile.");
                player_=p;
                trooperBrain_=std::make_unique<DStarLiteAgent>(world_,trooper_,player_);
                refreshPath();
                event("Player moved. Trooper goal changed, so D* Lite initialized a new goal graph.");
            } else if (name=="SET") {
                std::string key; double value;
                if(!(in>>key>>value) || !std::isfinite(value)) throw std::invalid_argument("Expected SET <health|ammo|exposure|stress> <number>");
                if(key=="health" && value>=0 && value<=1) health_=value;
                else if(key=="exposure" && value>=0 && value<=1) exposure_=value;
                else if(key=="stress" && value>=0 && value<=1) stress_=value;
                else if(key=="ammo" && value>=0 && value<=100 && std::floor(value)==value) ammo_=static_cast<int>(value);
                else throw std::invalid_argument("Out-of-range value or unknown setting.");
                event("Player " + key + " set to " + std::to_string(value) + ".");
            } else if (name=="SAFE") {
                int value=-1;
                if(!(in>>value) || (value!=0 && value!=1)) throw std::invalid_argument("Expected SAFE 0 or SAFE 1");
                safeZone_=value==1;
                event(safeZone_?"Safe zone enabled: aggressive encounters are restricted.":"Safe zone disabled.");
            } else throw std::invalid_argument("Unknown command: " + name);
            return snapshot();
        } catch(const std::exception& e) {return errorJson(e.what());}
    }
    std::string snapshot() const {
        const auto sPlan = securityBrain_.plan(securityFacts_);
        const auto dPlan = directorBrain_.choose(directorState());
        std::ostringstream out;
        out << std::setprecision(6);
        out << "{\"ok\":true,\"tick\":" << tick_ << ",\"lastEvent\":" << quote(lastEvent_);
        out << ",\"world\":{\"width\":"<<W<<",\"height\":"<<H<<",\"door\":"<<pt(DOOR)
            <<",\"doorOpen\":"<<(doorOpen_?"true":"false")<<",\"rows\":[";
        for(int y=0;y<H;++y) {
            if(y)out<<',';
            std::string line;
            for(int x=0;x<W;++x) line+=world_.blocked({x,y})?'#':'.';
            out<<quote(line);
        }
        out << "]},\"player\":"<<pt(player_)<<",\"security\":"<<pt(security_)
            <<",\"hollow\":"<<pt(hollow_)<<",\"trooper\":"<<pt(trooper_)
            <<",\"hollowTarget\":"<<pt(hollowTarget_);
        out << ",\"belief\":[";
        for(int i=0;i<world_.count();++i) {
            if(i)out<<',';
            out << hollowBrain_->probability(world_.point(i));
        }
        out << "],\"trooperPath\":";
        paths(out,trooperPath_);
        out << ",\"trooperExpansions\":" << expanded_;
        out << ",\"securityAI\":{\"lastAction\":"<<quote(securityLast_)
            <<",\"planCost\":"<<sPlan.cost<<",\"expanded\":"<<sPlan.expandedStates
            <<",\"achieved\":"<<(sPlan.achieved?"true":"false")<<",\"steps\":[";
        for(size_t i=0;i<sPlan.steps.size();++i) {if(i)out<<',';out<<quote(sPlan.steps[i]);}
        out << "]},\"director\":{\"chosen\":"<<quote(toString(dPlan.chosen))
            <<",\"lastTriggered\":"<<quote(toString(directorLast_))<<",\"scores\":[";
        for(size_t i=0;i<dPlan.scores.size();++i) {
            const auto& sc=dPlan.scores[i];
            if(i)out<<',';
            out<<"{\"name\":"<<quote(toString(sc.encounter))<<",\"utility\":"<<sc.utility
               <<",\"allowed\":"<<(sc.allowed?"true":"false")<<",\"reason\":"<<quote(sc.reason)<<"}";
        }
        out << "]},\"state\":{\"health\":"<<health_<<",\"ammo\":"<<ammo_
            <<",\"exposure\":"<<exposure_<<",\"stress\":"<<stress_
            <<",\"activeHollows\":"<<activeHollows_<<",\"cooldown\":"<<secondsSinceWave_
            <<",\"safeZone\":"<<(safeZone_?"true":"false")<<"},\"history\":[";
        for(size_t i=0;i<history_.size();++i) {if(i)out<<',';out<<quote(history_[i]);}
        out << "]}";
        return out.str();
    }
};
} // namespace

int main() {
    Simulation sim;
    std::cout << sim.snapshot() << '\n' << std::flush;
    std::string command;
    while (std::getline(std::cin,command)) {
        std::cout << sim.handle(command) << '\n' << std::flush;
    }
}
