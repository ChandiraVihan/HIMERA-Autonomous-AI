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
constexpr Point HAZARD{13, 5};

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
    bool doorOpen_ = true, safeZone_ = false, hazardOn_ = false;
    Point securityInterest_{7,2};
    int securityPatrol_ = 0, securityAmmo_ = 5;
    double securityHealth_ = 1.0;
    std::deque<int> hollowWaveExpiry_;
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
        doorOpen_ = true; hazardOn_ = false;
        securityInterest_ = {7,2}; securityPatrol_ = 0; securityAmmo_ = 5; securityHealth_ = 1.0;
        hollowWaveExpiry_.clear();
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
    static const char* booleanText(bool b) { return b ? "true" : "false"; }
    bool lineOfSight(Point from, Point to) const {
        // Conservative tile ray: a wall between an agent and target blocks sight.
        int x = from.x, y = from.y;
        int dx = std::abs(to.x-x), dy = std::abs(to.y-y);
        int sx = x < to.x ? 1 : -1, sy = y < to.y ? 1 : -1;
        int err = dx-dy;
        while (x != to.x || y != to.y) {
            int twice = 2*err;
            if (twice > -dy) { err -= dy; x += sx; }
            if (twice < dx) { err += dx; y += sy; }
            if (world_.blocked({x,y})) return false;
        }
        return true;
    }
    void moveSecurity(Point target) {
        auto path = route(world_, security_, target);
        if (path.size()>1) security_=path[1];
    }
    Point aimPosition() const {
        // Find a reachable, safe floor tile which offers an unobstructed shot.
        std::vector<int> dist(world_.count(), -1);
        std::queue<Point> q;
        q.push(security_); dist[world_.id(security_)] = 0;
        Point best = security_;
        int minScore = 100000;
        while(!q.empty()) {
            Point p=q.front();q.pop();
            int distance=dist[world_.id(p)];
            if (manhattan(p,player_)<=8 && lineOfSight(p,player_)) {
                int score=distance*2+manhattan(p,player_);
                if(score<minScore) {best=p;minScore=score;}
            }
            for (Point n:world_.neighbors(p)) if(dist[world_.id(n)]<0) {
                dist[world_.id(n)]=distance+1;q.push(n);
            }
        }
        return best;
    }
    void step() {
        ++tick_;
        secondsSinceWave_ += 5;
        while (!hollowWaveExpiry_.empty() && hollowWaveExpiry_.front()<=tick_) {
            hollowWaveExpiry_.pop_front();
            activeHollows_=static_cast<int>(hollowWaveExpiry_.size());
            event("An existing Hollow wave dissipated after 35 simulated seconds.");
        }
        hollowBrain_->predict(0.23);
        if (manhattan(hollow_,player_)<=2 && lineOfSight(hollow_,player_))
            hollowBrain_->seePlayer(player_);
        hollowTarget_ = hollowBrain_->selectSearchCell(hollow_);
        auto hp = route(world_, hollow_, hollowTarget_);
        if (hp.size() > 1) hollow_=hp[1];
        if (hollow_ != player_) hollowBrain_->observeEmpty({hollow_},0.08);

        // GOAP chooses the action; the simulator applies spatial actions only
        // after movement has actually reached the relevant destination.
        if (securityAmmo_<=0) securityFacts_ &= ~bit(Fact::HasAmmo);
        else securityFacts_ |= bit(Fact::HasAmmo);
        if (securityHealth_<0.3) securityFacts_ |= bit(Fact::LowHealth);
        else securityFacts_ &= ~(bit(Fact::LowHealth)|bit(Fact::Retreated));
        Plan plan = securityBrain_.plan(securityFacts_);
        securityLast_ = plan.steps.empty() ? "Observe" : plan.steps.front();
        bool done = false;
        if (securityLast_=="TakeCover") {
            Point cover{7,6};
            if (security_!=cover) moveSecurity(cover);
            done = security_==cover;
        } else if (securityLast_=="Retreat") {
            Point rally{2,2};
            if (security_!=rally) moveSecurity(rally);
            done = security_==rally;
        } else if (securityLast_=="InvestigateSound") {
            if (security_!=securityInterest_) moveSecurity(securityInterest_);
            done = security_==securityInterest_;
        } else if (securityLast_=="PatrolSector") {
            constexpr Point patrol[]={{7,2},{7,6},{2,8},{3,2}};
            Point target=patrol[securityPatrol_ % 4];
            if (security_!=target) moveSecurity(target);
            done = security_==target;
            if (done) {
                securityPatrol_=(securityPatrol_+1)%4;
                securityFacts_ &= ~bit(Fact::Patrolled);
            }
        } else if (securityLast_=="AcquireLineOfSight" || securityLast_=="SuppressTarget") {
            if (manhattan(security_,player_)>8 || !lineOfSight(security_,player_)) {
                securityLast_ = "PositionForShot";
                moveSecurity(aimPosition());
            } else done=true;
        } else if (securityLast_=="Reload") {
            securityAmmo_ = 6;
            done = true;
        }
        if (done) {
            securityFacts_ = securityBrain_.apply(securityFacts_, securityLast_);
            if (securityLast_=="SuppressTarget") {
                securityAmmo_ = std::max(0,securityAmmo_-1);
                event("Security suppressed the target using clear line of sight.");
            }
        }

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
            hollowWaveExpiry_.push_back(tick_+7);
            activeHollows_=static_cast<int>(hollowWaveExpiry_.size());
            secondsSinceWave_=0;
            event("HIM-7 triggered a 35-second Hollow wave; cooldown activated.");
        } else event("Simulation advanced 5 s. Security: " + securityLast_ +
                     "; Director: " + toString(choice.chosen) + ".");
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
                securityInterest_=player_;
                securityFacts_ |= bit(Fact::HeardSound);
                securityFacts_ &= ~(bit(Fact::SoundInvestigated)|bit(Fact::Patrolled));
                stress_=std::min(1.0,stress_+0.12);
                ammo_=std::max(0,ammo_-1);
                event("Gunshot: Hollow belief updated; Security hears an anomaly.");
            } else if (name=="SPOT") {
                securityInterest_=player_;
                securityFacts_ |= bit(Fact::KnowsTarget)|bit(Fact::UnderFire);
                securityFacts_ &= ~(bit(Fact::TargetSuppressed)|bit(Fact::Aimed));
                event("Security spotted the player. GOAP now plans a tactical response.");
            } else if (name=="LOST") {
                securityFacts_ &= ~(bit(Fact::KnowsTarget)|bit(Fact::UnderFire)|bit(Fact::Aimed)|bit(Fact::TargetSuppressed));
                securityFacts_ |= bit(Fact::HeardSound);
                securityFacts_ &= ~bit(Fact::SoundInvestigated);
                event("Security lost visual contact and is investigating the last known location.");
            } else if (name=="HAZARD") {
                hazardOn_=!hazardOn_;
                double risk=hazardOn_?22.0:0.0;
                world_.setRisk(HAZARD,risk);
                trooperBrain_->updateCell(HAZARD,false,risk);
                refreshPath();
                event(hazardOn_?"Chemical spill raised route risk; Trooper rerouted.":
                                "Chemical spill removed; Trooper repaired its route.");
            } else if (name=="DOOR") {
                doorOpen_=!doorOpen_;
                world_.setBlocked(DOOR,!doorOpen_);
                trooperBrain_->updateCell(DOOR,!doorOpen_);
                // Preserve probability learned from previous observations.
                hollowBrain_->setBlocked(DOOR,!doorOpen_);
                hollowTarget_=hollowBrain_->selectSearchCell(hollow_);
                refreshPath();
                event(std::string("Containment door ")+(doorOpen_?"opened":"closed")+
                      ". D* Lite repaired its graph; Hollow belief preserved across the change.");
            } else if (name=="PLAYER") {
                int x,y; if(!(in>>x>>y)) throw std::invalid_argument("Expected PLAYER x y");
                Point p{x,y}; if(!world_.walkable(p)) throw std::invalid_argument("Choose a walkable floor tile.");
                player_=p;
                // Moving the player does not magically update Security's knowledge.
                securityFacts_ &= ~(bit(Fact::KnowsTarget)|bit(Fact::UnderFire)|bit(Fact::Aimed)|bit(Fact::TargetSuppressed));
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
                else if(key=="securityHealth" && value>=0 && value<=1) securityHealth_=value;
                else if(key=="securityAmmo" && value>=0 && value<=30 && std::floor(value)==value) securityAmmo_=static_cast<int>(value);
                else throw std::invalid_argument("Out-of-range value or unknown setting.");
                if (key=="securityHealth") {
                    if (securityHealth_<0.3) securityFacts_ |= bit(Fact::LowHealth);
                    else securityFacts_ &= ~(bit(Fact::LowHealth)|bit(Fact::Retreated));
                }
                if (key=="securityAmmo") {
                    if (securityAmmo_>0) securityFacts_ |= bit(Fact::HasAmmo);
                    else securityFacts_ &= ~bit(Fact::HasAmmo);
                }
                event("Updated " + key + " set to " + std::to_string(value) + ".");
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
            <<",\"doorOpen\":"<<booleanText(doorOpen_)
            <<",\"hazard\":"<<pt(HAZARD)<<",\"hazardOn\":"<<booleanText(hazardOn_)
            <<",\"rows\":[";
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
            <<",\"health\":"<<securityHealth_<<",\"ammo\":"<<securityAmmo_
            <<",\"knowsTarget\":"<<booleanText(has(securityFacts_,Fact::KnowsTarget))
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
