// HIMERA v0.4: deterministic, auditable evaluations using the production C++ agents.
// The baselines are intentionally simple reference policies, not A* replacements.
#include "himera/goap.hpp"
#include "himera/hollow.hpp"
#include "himera/dstar.hpp"
#include "himera/director.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace himera;
namespace {
namespace fs = std::filesystem;
struct Writer {
    std::ofstream metrics;
    std::ofstream decisions;
    int count=0;
    explicit Writer(const fs::path& dir) : metrics(dir / "metrics.csv"), decisions(dir / "decisions.csv") {
        if (!metrics || !decisions) throw std::runtime_error("Cannot create evaluation CSV files");
        metrics << "agent,scenario,approach,metric,value,unit\n";
        decisions << "agent,scenario,approach,decision,explanation\n";
        metrics << std::setprecision(10);
    }
    static std::string csv(const std::string& s) {
        std::string out="\"";
        for (char ch:s) {if(ch=='\"') out += "\"\""; else if(ch=='\n')out+=' ';else out+=ch;}
        return out+"\"";
    }
    void measure(const std::string& a,const std::string& s,const std::string& v,const std::string& metric,double n,const std::string& unit) {
        if (!std::isfinite(n)) throw std::runtime_error("Nonfinite metric: " + a + " / " + s + " / " + metric);
        metrics << csv(a) << ',' << csv(s) << ',' << csv(v) << ',' << csv(metric) << ',' << n << ',' << csv(unit) << '\n';
        ++count;
    }
    void decision(const std::string& a,const std::string& s,const std::string& v,const std::string& action,const std::string& note) {
        decisions << csv(a) << ',' << csv(s) << ',' << csv(v) << ',' << csv(action) << ',' << csv(note) << '\n';
    }
};
std::string stepsText(const std::vector<std::string>& steps) {
    if(steps.empty()) return "(no actions)";
    std::string out;
    for (const auto& step:steps) { if(!out.empty())out += " -> "; out+=step; }
    return out;
}
Fact goalFor(Facts facts) {
    if(has(facts,Fact::LowHealth)) return Fact::Retreated;
    if(has(facts,Fact::KnowsTarget)) return Fact::TargetSuppressed;
    if(has(facts,Fact::HeardSound)) return Fact::SoundInvestigated;
    return Fact::Patrolled;
}
// Reference reactive policy: no forward search. It does not consider taking cover
// before reloading/aiming while under fire. This is a deliberately weak baseline.
std::string reactiveAction(Facts state) {
    if(has(state,Fact::LowHealth) && !has(state,Fact::Retreated)) return "Retreat";
    if(has(state,Fact::KnowsTarget)) {
        if(!has(state,Fact::HasAmmo)) return "Reload";
        if(!has(state,Fact::Aimed)) return "AcquireLineOfSight";
        return "SuppressTarget";
    }
    if(has(state,Fact::HeardSound)) return "InvestigateSound";
    return "PatrolSector";
}
double observedActionCost(const std::string& action,Facts state) {
    double cost=0;
    if(action=="TakeCover") cost=1;
    else if(action=="Reload" || action=="SuppressTarget")cost=2;
    else if(action=="InvestigateSound")cost=1.5;
    else if(action=="Retreat")cost=0.8;
    else cost=1;
    if(has(state,Fact::UnderFire) && !has(state,Fact::InCover)) {
        if(action=="Reload") cost+=8;
        if(action=="AcquireLineOfSight") cost+=6;
        if(action=="SuppressTarget") cost+=10;
    }
    return cost;
}
void goapEvaluation(Writer& out) {
    SecurityGoapAgent agent;
    const auto F=[](Fact f){return bit(f);};
    struct Case {const char* name;Facts state;};
    const std::vector<Case> cases={
        {"peaceful_patrol",0},
        {"unknown_gunshot",F(Fact::HeardSound)},
        {"visible_target_ready",F(Fact::KnowsTarget)|F(Fact::HasAmmo)},
        {"under_fire_no_ammo",F(Fact::KnowsTarget)|F(Fact::UnderFire)},
        {"under_fire_loaded",F(Fact::KnowsTarget)|F(Fact::UnderFire)|F(Fact::HasAmmo)},
        {"critical_health",F(Fact::KnowsTarget)|F(Fact::UnderFire)|F(Fact::LowHealth)},
        {"already_suppressed",F(Fact::KnowsTarget)|F(Fact::HasAmmo)|F(Fact::Aimed)|F(Fact::TargetSuppressed)}
    };
    for(const auto& c:cases){
        auto begin=std::chrono::steady_clock::now();
        Plan plan=agent.plan(c.state);
        auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
        Facts state=c.state;
        for(const auto& a:plan.steps) state=agent.apply(state,a);
        bool valid=plan.achieved && has(state,goalFor(c.state));
        if(!valid) throw std::runtime_error(std::string("GOAP invalid plan in ")+c.name);
        out.measure("Security",c.name,"GOAP","goal_reached",valid, "boolean");
        out.measure("Security",c.name,"GOAP","action_count",plan.steps.size(),"actions");
        out.measure("Security",c.name,"GOAP","plan_cost",plan.cost,"cost units");
        out.measure("Security",c.name,"GOAP","expanded_states",plan.expandedStates,"states");
        out.measure("Security",c.name,"GOAP","planning_us",elapsed,"microseconds");
        out.decision("Security",c.name,"GOAP",stepsText(plan.steps),"Lowest-cost achievable symbolic plan");
        state=c.state;double cost=0;int steps=0;std::vector<std::string> actions;
        while(!has(state,goalFor(c.state)) && steps<10){
            std::string action=reactiveAction(state);
            cost+=observedActionCost(action,state);
            state=agent.apply(state,action);
            ++steps; actions.push_back(action);
        }
        const bool success=has(state,goalFor(c.state));
        out.measure("Security",c.name,"Reactive rules","goal_reached",success,"boolean");
        out.measure("Security",c.name,"Reactive rules","action_count",steps,"actions");
        out.measure("Security",c.name,"Reactive rules","plan_cost",cost,"cost units");
        out.decision("Security",c.name,"Reactive rules",stepsText(actions),"Greedy action policy, no look-ahead");
    }
}
int bfsDistance(const Grid& g, Point start, Point finish) {
    if(!g.walkable(start)||!g.walkable(finish))return -1;
    std::vector<int> d(g.count(),-1); std::queue<Point> q; d[g.id(start)]=0;q.push(start);
    while(!q.empty()){Point p=q.front();q.pop();if(p==finish)return d[g.id(p)];
        for(Point n:g.neighbors(p))if(d[g.id(n)]<0){d[g.id(n)]=d[g.id(p)]+1;q.push(n);}}
    return -1;
}
std::string pointText(Point p){return "("+std::to_string(p.x)+","+std::to_string(p.y)+")";}
void hollowEvaluation(Writer& out) {
    struct Case {const char* name;Point self;Point player;Point sound;bool wall;bool empty;bool ally;bool hazard;};
    std::vector<Case> cases={
        {"clear_sound",{1,3},{7,3},{7,2},false,false,false,false},
        {"blocked_sound_origin",{1,3},{7,3},{4,3},true,false,false,false},
        {"negative_visual_evidence",{1,3},{7,3},{7,2},false,true,false,false},
        {"coordinated_search",{1,3},{6,5},{6,4},false,false,true,false},
        {"hazard_avoidance",{1,3},{7,3},{6,3},false,false,false,true},
        {"isolated_region",{1,3},{1,5},{7,3},true,false,false,false}
    };
    for(const auto& c:cases){
        Grid grid(10,7);
        if(c.wall){ for(int y=0;y<7;++y)grid.setBlocked({4,y},true); }
        if(c.hazard) for(int y=2;y<=4;++y)grid.setRisk({6,y},5);
        HollowBeliefAgent agent(grid);
        agent.hear(c.sound,1.6,0.92);
        if(c.empty)agent.observeEmpty({c.sound},0.01);
        std::vector<Point> allies = c.ally ? std::vector<Point>{{6,4}} : std::vector<Point>{};
        auto before=std::chrono::steady_clock::now();
        Point selected=agent.selectSearchCell(c.self,allies);
        auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-before).count();
        if (std::abs(agent.totalProbability()-1.0)>1e-7) throw std::runtime_error("Hollow posterior invalid");
        // Baseline: walk straight to unmodified perceived sound coordinate.
        // No posterior, risk avoidance or reachability check.
        for(const auto& approach: {std::string("Belief + reachability"),std::string("Raw sound target")}){
            Point target=approach=="Belief + reachability"?selected:c.sound;
            const int travel=bfsDistance(grid,c.self,target);
            out.measure("Hollow",c.name,approach,"reachable",travel>=0,"boolean");
            out.measure("Hollow",c.name,approach,"distance_to_player",manhattan(target,c.player),"Manhattan cells");
            out.measure("Hollow",c.name,approach,"travel_steps",travel,"steps (-1 unreachable)");
            out.measure("Hollow",c.name,approach,"destination_risk",grid.risk(target),"risk units");
            out.measure("Hollow",c.name,approach,"posterior_at_target",agent.probability(target),"probability");
            if(approach=="Belief + reachability")out.measure("Hollow",c.name,approach,"planning_us",elapsed,"microseconds");
            out.decision("Hollow",c.name,approach,pointText(target),approach=="Belief + reachability"?"Highest reachable posterior score adjusted for travel, risk and allies":"Unadjusted sound report");
        }
    }
}
struct SearchResult{double cost;int expansions;};
// Independent fresh A* reference. The cost model matches the D* graph:
// entering cell n costs (1 + risk(n)), with Manhattan as an admissible heuristic.
SearchResult astar(const Grid& g,Point start,Point goal){
    constexpr double INF=std::numeric_limits<double>::infinity();
    if(!g.walkable(start)||!g.walkable(goal))return {INF,0};
    struct Node{double f,g;int id;bool operator>(const Node& b)const {if(f!=b.f)return f>b.f;return id>b.id;}};
    std::priority_queue<Node,std::vector<Node>,std::greater<>> open;
    std::vector<double> dist(g.count(),INF);dist[g.id(start)]=0;open.push({double(manhattan(start,goal)),0,g.id(start)});
    int expanded=0;
    while(!open.empty()){
        auto node=open.top();open.pop();if(node.g>dist[node.id]+1e-9)continue;
        ++expanded;Point p=g.point(node.id);
        if(p==goal)return {node.g,expanded};
        for(Point n:g.neighbors(p)){
            double next=node.g+1+g.risk(n);int id=g.id(n);
            if(next+1e-9<dist[id]){dist[id]=next;open.push({next+manhattan(n,goal),next,id});}
        }
    }
    return {INF,expanded};
}
double pathCost(const Grid& g,const std::vector<Point>& path,Point start,Point goal){
    if(path.empty())return std::numeric_limits<double>::infinity();
    if(path.front()!=start || path.back()!=goal)throw std::runtime_error("D* returned incorrect endpoints");
    double cost=0;
    for(size_t i=1;i<path.size();++i){
        if(manhattan(path[i-1],path[i])!=1||!g.walkable(path[i]))throw std::runtime_error("D* path has invalid edge");
        cost+=1+g.risk(path[i]);
    }
    return cost;
}
void dstarEvaluation(Writer& out){
    struct Case {const char* name;int mode;};
    for(auto c:{Case{"changing_doors",0},Case{"changing_hazards",1},Case{"mixed_updates",2}}){
        Grid grid(24,16);Point start{1,7},goal{22,7};
        for(int y=1;y<=14;++y)if(y!=4&&y!=11)grid.setBlocked({12,y},true);
        DStarLiteAgent agent(grid,start,goal);
        for(int step=0;step<15;++step){
            // Vary one portal, one risk field, and repeat updates across trials.
            const Point door = step%2==0 ? Point{12,4}:Point{12,11};
            Point hazard{8+(step%3), 3+(step%7)};
            if(c.mode==0){
                bool blocked=(step/2)%2==0;
                grid.setBlocked(door,blocked);agent.updateCell(door,blocked,0);
            }else if(c.mode==1){
                double risk= (step%3==0)?8.0:0.0;
                grid.setRisk(hazard,risk);agent.updateCell(hazard,false,risk);
            }else{
                if(step%2==0){bool blocked=step%4==0;grid.setBlocked(door,blocked);agent.updateCell(door,blocked,0);}
                else {double risk=step%3==0?15.:0.;grid.setRisk(hazard,risk);agent.updateCell(hazard,false,risk);}
            }
            auto t1=std::chrono::steady_clock::now();
            auto path=agent.findPath();
            auto t2=std::chrono::steady_clock::now();
            auto baseline=astar(grid,start,goal);
            auto t3=std::chrono::steady_clock::now();
            double cost=pathCost(grid,path,start,goal);
            if(!std::isfinite(cost)||std::abs(cost-baseline.cost)>1e-7){
                throw std::runtime_error(std::string("D* / A* disagree at ")+c.name+" step "+std::to_string(step));
            }
            const auto s=std::string(c.name)+"_step_"+std::to_string(step);
            for(const auto& variant:{std::string("D* Lite"),std::string("Fresh A*")}){
                const bool d=variant=="D* Lite";
                out.measure("Trooper",s,variant,"path_cost",d?cost:baseline.cost,"cost units");
                out.measure("Trooper",s,variant,"expanded_states",d?agent.lastExpansionCount():baseline.expansions,"states");
                out.measure("Trooper",s,variant,"planning_us",d?std::chrono::duration<double,std::micro>(t2-t1).count():std::chrono::duration<double,std::micro>(t3-t2).count(),"microseconds");
                out.measure("Trooper",s,variant,"optimal_path",1,"boolean");
                out.decision("Trooper",s,variant,"cost="+std::to_string(cost),d?"Incremental shortest-path repair":"Fresh shortest-path search");
            }
        }
    }
}
// Fairness is evaluated against the Director agent's documented constraints,
// not an arbitrary win-rate: a wave requires adequate health, ammo, exposure,
// cooldown and enemy capacity, and must not occur inside a safe zone.
bool waveAllowed(const DirectorState& s){
    return !s.safeZone&&s.health01>=.35&&s.ammo>=5&&s.activeHollows<4&&s.secondsSinceWave>=25&&s.exposure01>=.4;
}
void directorEvaluation(Writer& out){
    struct Case {const char* name;DirectorState s;};
    std::vector<Case> cases={
        {"calm",{.health01=.85,.ammo=20,.exposure01=.1,.stress01=.2,.activeHollows=0,.secondsSinceWave=100,.safeZone=false}},
        {"exposed_ready",{.health01=.9,.ammo=20,.exposure01=.9,.stress01=.6,.activeHollows=0,.secondsSinceWave=100,.safeZone=false}},
        {"safe_zone",{.health01=.9,.ammo=20,.exposure01=.95,.stress01=.7,.activeHollows=0,.secondsSinceWave=100,.safeZone=true}},
        {"critical_health",{.health01=.2,.ammo=20,.exposure01=.95,.stress01=.8,.activeHollows=0,.secondsSinceWave=100,.safeZone=false}},
        {"no_ammo",{.health01=.9,.ammo=0,.exposure01=.95,.stress01=.8,.activeHollows=0,.secondsSinceWave=100,.safeZone=false}},
        {"cooldown",{.health01=.9,.ammo=20,.exposure01=.9,.stress01=.7,.activeHollows=0,.secondsSinceWave=5,.safeZone=false}},
        {"enemy_cap",{.health01=.9,.ammo=20,.exposure01=.95,.stress01=.7,.activeHollows=4,.secondsSinceWave=100,.safeZone=false}},
        {"below_threshold",{.health01=.85,.ammo=20,.exposure01=.25,.stress01=.7,.activeHollows=0,.secondsSinceWave=100,.safeZone=false}}
    };
    ExposureDirectorAgent director;
    for(const auto& c:cases){
        auto start=std::chrono::steady_clock::now();
        auto result=director.choose(c.s);
        double elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        Encounter fixed=c.s.exposure01>=.45?Encounter::HollowWave:(c.s.exposure01>=.15?Encounter::VisualDistortion:Encounter::Quiet);
        for(const auto& variant:{std::string("Utility AI"),std::string("Fixed threshold")}){
            const bool adaptive=variant=="Utility AI";
            Encounter choice=adaptive?result.chosen:fixed;
            const bool violation=(choice==Encounter::HollowWave&&!waveAllowed(c.s))||
                                 (choice==Encounter::VisualDistortion&&(c.s.safeZone||c.s.exposure01<.15));
            if(adaptive&&violation)throw std::runtime_error(std::string("Director fairness violation: ")+c.name);
            out.measure("Director",c.name,variant,"constraint_violations",violation,"count");
            out.measure("Director",c.name,variant,"wave_selected",choice==Encounter::HollowWave,"boolean");
            if(adaptive)out.measure("Director",c.name,variant,"planning_us",elapsed,"microseconds");
            out.decision("Director",c.name,variant,toString(choice),adaptive?"Utility maximization under explicit event constraints":"Only threshold on exposure; ignores safety and cooldown");
        }
        for(const auto& score:result.scores)
            out.measure("Director",c.name,"Utility AI","score_"+toString(score.encounter),score.utility,"utility");
    }
}
} // namespace
int main(int argc,char** argv){
    try {
        fs::path dir="evaluation-results";
        if(argc==3&&std::string(argv[1])=="--out")dir=argv[2];
        else if(argc!=1)throw std::invalid_argument("Usage: himera_evaluate [--out OUTPUT_FOLDER]");
        fs::create_directories(dir);
        Writer w(dir);
        goapEvaluation(w);
        hollowEvaluation(w);
        dstarEvaluation(w);
        directorEvaluation(w);
        std::cout<<"HIMERA v0.4 evaluation finished: "<<w.count<<" metric records in "<<dir.string()<<"\n";
        std::cout<<"Saved metrics.csv and decisions.csv. Run evaluation/make_report.py to create report.html.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"EVALUATION FAILED: "<<e.what()<<'\n';return 1;}
}
