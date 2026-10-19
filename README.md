# HIMERA Autonomous Agents + Visual Simulator (C++ Prototypes)

Advanced autonomous decision systems for **HIMERA**, the psychological horror FPS set in fictional Research Complex K-47. This repository focuses on the **SE3062 Intelligent Systems** assignment. The Unreal Engine 5 gameplay project remains separate at this stage.

> **Current status:** Tested standalone C++ prototypes plus a working browser-based 2D scenario simulator. The simulator is not yet an Unreal Engine game, and the agents are not yet exposed to Blueprints.

## NEW: Interactive 2D agent simulator (v0.2)

![Himera AI simulator screenshot](docs/images/visual_simulator_preview.png)

The repository now includes an interactive **browser-based visual lab**, powered by the **existing C++ algorithms** rather than rewriting them in JavaScript. It displays the Trooper's D* Lite route, the Hollow's search probability, Security's GOAP sequence, and the HIM-7 Director's utility scores. You can close a door, fire a gunshot, reveal the player, adjust health/ammo/exposure/stress, move the player by clicking, and advance simulation time.

**Windows (VS Code supported; install MSYS2 UCRT64 GCC/CMake/Ninja/Python, no Visual Studio IDE required):**

```powershell
powershell -ExecutionPolicy Bypass -File .\visual\START_WINDOWS.ps1
```

**Manual start (after building CMake):** `py -3 visual/server.py` (Windows) or `python3 visual/server.py` (Linux/macOS), then open **http://127.0.0.1:8765**.

Read [docs/VISUAL_SIMULATOR.md](docs/VISUAL_SIMULATOR.md) for full installation, experiment steps, constraints and architecture. The Visual Simulator is a **standalone 2D lab, not an Unreal preview**.

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
- A C++20 compiler (MSYS2 UCRT64 GCC or Visual Studio C++ Build Tools on Windows; Clang on macOS; GCC/Clang on Linux)
- No third-party libraries for native C++ code; the visual simulator additionally needs Python 3.9+ (standard library only)

## Build and run

### Windows + VS Code (recommended: MSYS2 UCRT64, no Visual Studio installation)

1. Install [MSYS2](https://www.msys2.org/) into the default `C:\msys64` path.
2. Open **MSYS2 UCRT64** (not MSYS, MinGW64, or CLANG64).
3. Run `pacman -Syu` and restart the UCRT64 terminal if requested.
4. Install tools (in MSYS2 UCRT64):

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-python
   ```

5. Open this repository in **VS Code → Terminal → New Terminal → PowerShell**. Run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\visual\START_WINDOWS.ps1
   ```

The launcher discovers MSYS2 GCC, uses **Ninja** and an isolated `build-vscode` directory, and launches the browser at http://127.0.0.1:8765. MSYS2 C++ toolchain DLLs are supplied to the simulator process via PATH. To run the tests, use: 

```powershell
& "C:\msys64\ucrt64\bin\ctest.exe" --test-dir build-vscode --output-on-failure
```

If MSYS2 is installed outside `C:\msys64`, set `$env:MSYS2_ROOT` to that install directory before launching.

### Windows + Visual Studio C++ Build Tools (alternative)

The same launcher also supports Visual Studio 2022 C++ Build Tools if MSYS2 is absent. The full Visual Studio IDE is not needed. Manual build commands:

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

### Visual simulator after building

```powershell
py -3 visual/server.py
```

The local web UI opens automatically. Use the `START_WINDOWS.ps1` launcher for a one-command build + launch.

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
