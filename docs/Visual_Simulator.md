# Visual Simulator v0.2 — HIMERA C++ Agent Laboratory

This is a **real 2D scenario adapter** for the four C++ agent algorithms. The browser does **not** reimplement GOAP, belief tracking, D* Lite or utility selection in JavaScript: all planning calls execute in `himera_sim` and the browser renders JSON snapshots.

## How the three parts connect

```text
Browser (visual/web/index.html + app.js + styles.css)
  │  GET /api/state and POST /api/action
  ▼
Python standard-library local HTTP server (visual/server.py)
  │  single text command / one JSON snapshot, persistent subprocess
  ▼
C++ world adapter (visual/simulator.cpp)
  ├── SecurityGoapAgent       ← unchanged include/ + src/ code
  ├── HollowBeliefAgent      ← unchanged include/ + src/ code
  ├── DStarLiteAgent         ← unchanged include/ + src/ code
  └── ExposureDirectorAgent  ← unchanged include/ + src/ code
```

**Windows requirements:** MSYS2 UCRT64 GCC/CMake/Ninja/Python (recommended for VS Code), OR Visual Studio 2022 C++ Build Tools with CMake/Python. The VS Code editor alone is not a compiler.

**Install CMake (PowerShell):**

```powershell
winget install --id Kitware.CMake -e
```

After installation, close and reopen PowerShell. Confirm `cmake --version` and `py -3 --version`.

### MSYS2 UCRT64 setup for VS Code

Install [MSYS2](https://www.msys2.org/); launch the **MSYS2 UCRT64** terminal, then run:

```bash
pacman -Syu
# Restart UCRT64 if the updater asks, then:
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-python
```

The launcher prefers MSYS2 + Ninja and outputs to `build-vscode`, avoiding the previous `build` directory and any Visual Studio generator caches. It also supports Visual Studio C++ Build Tools as a fallback.

### One-command Windows launcher

From the repository root (no administrator privilege necessary):

```powershell
powershell -ExecutionPolicy Bypass -File .\visual\START_WINDOWS.ps1
```

The script builds `himera_sim.exe` if missing and opens the browser at **http://127.0.0.1:8765**. Press **Ctrl+C** in the terminal to stop the server. If port 8765 is occupied, manually launch `py -3 visual/server.py --port 8766`.

### Manual Windows commands (Visual Studio toolchain only)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
py -3 visual/server.py
```

### Linux and macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
python3 visual/server.py
```

Only Python standard-library modules are used: **no Flask, FastAPI, npm, pip or Unreal Engine installation required** for this demo. The UI loads optionally hosted fonts; system-font fallbacks work offline.

## Experiment walkthrough

1. Initially, the Trooper's orange dashed D* Lite path passes through the green door. Press **Close door**. The route changes from **16 to 20 cells** on the included scenario. Reopen it to restore the shorter path.
2. Press **Fire gunshot**, then **Hollow belief**: the red probability heat map concentrates near the player's position. The Hollow's estimated search tile is shown in the right panel.
3. Press **Spot player**: the Security agent's action list changes from `PatrolSector` to a multi-step GOAP plan (`TakeCover`, `AcquireLineOfSight`, `Reload`, `SuppressTarget`).
4. Press **Advance 5 seconds** or enable **Auto advance** to move the three physical agents one grid cell per step and execute Security's current symbolic action.
5. Set Health to **20%**. The Director's HollowWave utility is now marked **BLOCKED** because of its health constraint. Set Safe Zone on to disable distortion as well.
6. Click a floor tile to change the player destination. This intentionally **reinitializes** D* Lite for the **new goal**; opening/closing the door instead **repairs the existing** search graph.
7. Use **Reset** to return to the initial scenario.

## Boundaries and limitations to explain at the viva

- The scenario moves markers one grid cell per tick. It is a **decision-logic visualization**, not a faithful animation, collision, damage, field-of-view or shooting simulation.
- GOAP plans action sequences; the adapter animates a simplified movement policy and immediately applies the first symbolic action per tick. It does **not** claim every symbolic action has a detailed world-side effect.
- Hollow tracking is a probability distribution updated by a perceived gunshot, movement diffusion and negative evidence. When the door topology changes, v0.1's immutable belief-grid model must be reconstructed, which clears previous observations. This is visible in the event log.
- D* Lite can reuse work when **cell costs change with a fixed goal**. When the **goal itself changes**, this prototype reconstructs the planner. Last-expansion statistics are per most recent C++ search call; in some steps they are zero if the solution remains consistent.
- The Exposure Director calculates scores on every request but *triggers* its selected action only on a time step. The cooldown is then updated. A HollowWave increases a counter; the visual demo does not actually spawn another humanoid actor.
- Safe zone is a controlled experiment input rather than an automatically detected map region.
- All four C++ algorithms are still **prototypes**. You must integrate the AI with Unreal Blueprints, NavMesh/actors, movement, enemies, triggers and the real game environment for the assignment.

## Protocol notes

- `himera_sim` reads one command per line on standard input and writes one complete JSON document per line to standard output, beginning with its initial snapshot.
- Commands: `STEP`, `GUNSHOT`, `SPOT`, `DOOR`, `RESET`, `SAFE 0|1`, `PLAYER x y`, `SET health|ammo|exposure|stress value`.
- `visual/server.py` exposes **only 127.0.0.1**, checks command formats and request sizes. Do not deploy it publicly without authentication and hardening.
- UI is stateless except for the current view and auto-tick timer; the persistent C++ process owns the live scenario state.

## Testing

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Includes existing `AgentsUnitTests`, a CMake command-protocol smoke test, and (when Python 3 is installed) functional tests verifying dynamic route changes, GOAP response, sound-driven belief and the Director's fairness rules.
