# HIMERA Autonomous Agents (Standalone C++ Prototype)

Advanced autonomous decision systems for **HIMERA**, the psychological horror FPS set in fictional Research Complex K-47. This repository focuses on the **SE3062 Intelligent Systems** assignment. The Unreal Engine 5 gameplay project remains separate at this stage.

> **Current status:** First **standalone tested C++ prototypes**. These are algorithm demonstrations, not playable NPCs, not finished production AI, and not yet exposed to Blueprints.

## The four independently owned agents

| Agent | Algorithm | Demonstration | Unreal destination |
|---|---|---|---|
| Security Soldier | GOAP, cost-based symbolic planning | Chooses cover → reload → aim → suppress using action preconditions/costs | AI Controller + perception + EQS |
| Hollow | Grid probability belief filter + sound likelihood + search-area scoring | Updates probable player location from sound and negative visual evidence | AI Perception + NPC search movement |
| Containment Trooper | **D* Lite** incremental graph search | Replans when a door is closed or a danger zone changes cost | Nav graph adapter + path waypoints |
| HIM-7 Exposure Director | Multi-objective utility selection with fairness constraints | Controls when radio whispers, distortion or Hollow waves occur | World subsystem / Blueprint actor |

**Why four?** The provided assignment PDF requires exactly four students and one distinct autonomous agent per person, even though a later project discussion mentioned three. Confirm any updated requirements with your lecturer.

## Requirements

- CMake 3.20 or later
- A C++20 compiler (Visual Studio 2022 on Windows, Clang on macOS, GCC/Clang on Linux)
- No third-party libraries

## Build and run

### Windows (PowerShell, with Visual Studio C++ build tools installed)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\himera_demo.exe
.\build\Release\himera_benchmark.exe
```

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/himera_demo
./build/himera_benchmark
```

## Standalone benchmark

Run `himera_benchmark` to compare D* Lite incremental repairs against a fresh A* calculation on the **same synthetic changing map**, including correctness checks. The figures are illustrative, environment-dependent, and not evidence of an Unreal Engine FPS improvement.

## Development responsibilities and branch names

- Member 1: `feature/agent-security-goap`
- Member 2: `feature/agent-hollow-belief`
- Member 3: `feature/agent-trooper-dstar`
- Member 4: `feature/agent-exposure-director`

Start with the already tested library and work on independent branches. Use small, descriptive commits. Each member should own design decisions, test scenarios, evaluation evidence, and viva explanation for their agent.

## Design boundary for Unreal integration

The files in `include/` and `src/` use only standard C++, keeping algorithm logic independent from Unreal actors, animation assets, and scenes. The **planned** Unreal-facing module will translate engine observations into C++ inputs and decisions back into Blueprint-accessible commands. That adapter has **not** yet been implemented or tested in Unreal 5.8.

- Goap input: symbolic world facts; output: ordered actions (take cover, aim, reload, etc.).
- Hollow input: map and noisy observations; output: probable search target tile.
- Trooper input: grid, blocked cells, risk costs; output: waypoints.
- Director input: gameplay health/ammo/exposure/cooldown indicators; output: event type and explainable scores.

**Important:** D* Lite runs on its **own graph**. It doesn't replace Unreal's NavMesh automatically. An adapter must map doors/hazards to graph cells and translate route points to UE world coordinates before issuing movement requests. The grid model is a controlled prototype; move to room/portal graphs or an intentionally authored waypoint graph if a full 3D NavMesh topology is too large.

## Algorithmic justification

Read [docs/ALGORITHMS.md](docs/ALGORITHMS.md) for model equations, limitations, extension tasks and viva defense. These approaches solve **different problems**. No claim is made that they are universally faster or more optimal than A*.

## Work to complete before submission

1. Add game-specific tactical state and equipment constraints to GOAP; validate action plans in live Unreal gameplay.
2. Add walkability-aware sound attenuation/occlusion to the Hollow belief filter, and verify searching does not get stuck at unreachable cells.
3. Connect the Trooper's incremental planner to real door triggers and changing navigation costs. Benchmark against full A* or Dijkstra replanning with repeatable maps.
4. Give the director persistent cooldown/tension state and actual spawn/effect integration; tune utility coefficients in playtesting.
5. Connect all four simultaneously to Unreal 5.8, ensure stable animation, collision and gameplay, and profile frame time.
6. Preserve each member's own development commits, measurements and technical defense.

The standalone C++ library is only the beginning of the Intelligent Systems project, **not proof that the full rubric has been met**.
