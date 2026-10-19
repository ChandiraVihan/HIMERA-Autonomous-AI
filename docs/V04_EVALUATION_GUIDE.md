# HIMERA v0.4 — Evaluation & Viva Evidence

This is a **source-based evaluation harness**, not a substitute for the Unreal assignment's 3D NPC integration. All four advanced algorithm results are computed by the existing C++ library (`src/*.cpp`); baseline behavior and evaluation scenarios are implemented separately in `evaluation/evaluate.cpp`. The report is produced offline by the standard-library Python script `evaluation/make_report.py`.

## Windows / VS Code, MSYS2 UCRT64

From the repository folder, create `feature/v0.4-ai-evaluation` from the already verified `feature/v0.3-reactive-agents` branch, apply the upgrade files, then run:

```powershell
cd "D:\Y3S1\GV\Himera-Autonomous-AI"
$env:MSYS2_ROOT = "D:\MSYS2"
powershell -ExecutionPolicy Bypass -File .\evaluation\RUN_WINDOWS.ps1
```

The script builds with GCC + Ninja into `build-vscode`, runs **all CTest suites**, creates `evaluation-results/metrics.csv`, `evaluation-results/decisions.csv`, and opens `evaluation-results/report.html` in your default browser. It does not require Visual Studio, Node.js or pip packages. The existing interactive simulator is still launched separately with `visual/START_WINDOWS.ps1`.

If launching manually:

```powershell
$env:PATH = "D:\MSYS2\ucrt64\bin;$env:PATH"
& "D:\MSYS2\ucrt64\bin\cmake.exe" --build build-vscode --config Release
& .\build-vscode\himera_evaluate.exe --out evaluation-results
& "D:\MSYS2\ucrt64\bin\python.exe" evaluation\make_report.py --input evaluation-results
start .\evaluation-results\report.html
```

## Scenarios and honest baselines

| Agent | Advanced method | Reference baseline | Same-world outcome | Important limitation |
|---|---|---|---|---|
| Security | GOAP priority-queue lowest-cost state search | Greedy rule policy, no look-ahead or proactive cover | Goal reached, symbolic action count/cost, expanded states | Costs are authored penalties, not hitpoints lost or frame time. Baseline is deliberately weaker. |
| Hollow | Sound-weighted posterior, reachability-aware search, ally/risk scoring | Go to reported sound origin directly | Walkable/reachable destination; posterior, risk, distance to hidden player | Synthetic 10×7 grid; no real acoustic occlusion, goal success or player motion realism. |
| Trooper | D* Lite graph repair | Fresh A* after each change | Same weighted-grid path cost, path validity, expansions, execution time | Small synthetic grid, single-timing observations; fewer expansions **do not** necessarily mean faster runtime. |
| Director | Constraint-aware utility chooser | Exposure-only fixed event threshold | Fairness constraint violations, wave selection and decision explanations | Constraints defined by current project design, not clinical/gameplay enjoyment metrics. |

### Definitions

- **Planning cost, Security:** sum of each action's defined symbolic cost; reload/aim/suppress receive extra exposure penalties while under fire outside cover. Both approaches use the same cost schedule. This is *not* elapsed time.
- **Reachability, Hollow:** BFS path exists from current agent tile to selected destination on the current walkability grid. `travel_steps=-1` denotes an unreachable destination.
- **Path cost, Trooper:** sum of `1 + destination_cell_risk` for each grid step. Manhattan heuristic remains admissible because every edge costs at least one. Each D* path is checked against independently implemented A*.
- **Constraint violation, Director:** wave in safe zone, player health below 0.35, ammo below 5, active Hollows >= 4, cooldown under 25s, or exposure under 0.4; or distortion in a safe zone/under exposure threshold.
- **planning_us:** one observed wall-clock timing (microseconds) on the executing system. Not repeatable and excluded from deterministic regression comparisons. Before comparing performance, rerun many trials and report median, spread, compiler, CPU and load.

## What was tested

The evaluation consists of **7 GOAP conditions, 6 Hollow observations, 3×15 D* replanning steps, and 8 Director states**. Each scenario produces metric rows and explicit decision traces. `evaluation/test_evaluation.py` runs the native benchmark twice, compares all non-time results exactly, validates invariants, confirms that decisions match, and verifies the report is generated. The project continues to run existing tests.

**Guardrails:** no stochastic claims, no NPC frame-rate claims, no universal "better than A*" claims. All reported outcomes are conditional on these scenarios, maps, cost definitions and baselines.

## Viva walkthrough (5 minutes)

1. Open `evaluation-results/report.html` and explain each baseline in one sentence.
2. Show Security's `under_fire_no_ammo`: compare GOAP's plan with greedy reloading while exposed.
3. Show the Hollow's `blocked_sound_origin`: an attractive acoustic coordinate can be unreachable.
4. Show Trooper expansions *and* measured runtime separately. Discuss why D* Lite repairs can visit fewer nodes yet execute slower than A* on a small map.
5. Show Director's `safe_zone`, `critical_health`, and `cooldown` cases: utility gating prevents unsafe encounter selections.
6. Open `evaluation/evaluate.cpp` during Q&A and be ready to justify the exact score, constraints, heuristic and graph structure.
7. Finish by explaining that final assignment integration requires four concurrent Unreal 5 NPC/environment agents and profiling of gameplay frame time.

## Commit and push (after confirming locally)

```powershell
git status
git add .gitignore CMakeLists.txt evaluation docs/V04_EVALUATION_GUIDE.md
git commit -m "feat(ai): add reproducible evaluation dashboard and baselines"
git push -u origin feature/v0.4-ai-evaluation
```

The `evaluation-results/` directory is gitignored intentionally: results can change between machines. If you want to submit measurements, attach a clearly labeled dated snapshot with CPU/compiler details or include it as a release artifact, rather than implying runtime measurements are universal.
