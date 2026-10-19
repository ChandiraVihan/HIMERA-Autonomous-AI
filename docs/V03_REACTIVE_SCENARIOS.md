# v0.3 — Reactive Scenario Lab

This milestone extends the **same** four C++ decision modules from the v0.2 visual simulator. It does not create Unreal NPCs, animated characters, or any claim of superiority over A*.

## How to try it

Open the existing `visual/START_WINDOWS.ps1` launcher in VS Code on Windows. The browser remains at `http://127.0.0.1:8765`. From the HIMERA AI Lab:

1. **Fire gunshot, then Step**: the Hollow updates its belief and seeks likely player locations. Security investigates the sound but does **not** learn that the player is visible. This removes v0.2's false knowledge shortcut. It completes its investigation only after moving to the reported sound tile.
2. **Spot player, then Step**: GOAP plans tactical cover/aim/suppression. Movement-based actions only complete on arrival; firing requires a clear tile-based line of sight within eight Manhattan cells. **Lose player** makes Security investigate its last known position. Drag **Guard health** below 30% to prioritize retreat. Set **Guard ammunition** to zero to trigger reload when attacking.
3. **Activate chemical spill**: the Trooper uses D* Lite to avoid a high-cost but *walkable* hazardous cell (13,5). Clear spill to recover the shortest route; close/open the door at (9,5) to explore blocked topology changes.
4. **Fire gunshot, change door**: the Hollow belief posterior is maintained across the door change, rather than reset to a uniform distribution. Search targets are restricted to reachable cells.
5. **Advance time repeatedly**: HIM-7 waves have a finite lifetime (35 simulation seconds); encounter cooldowns, active-wave limits and safe zone still apply. Compare utility scores as player health, ammo, stress and exposure change.

## Implementation notes and limitations

- C++ core: `src/goap.cpp`, `src/hollow.cpp`, `src/dstar.cpp`, `src/director.cpp`. No browser-side AI is substituted.
- Scenario adapter: `visual/simulator.cpp` translates manual events to agent inputs and moves discrete grid NPCs one tile per 5-second simulation step.
- Browser interface: `visual/web/index.html`, `visual/web/app.js`, `visual/web/styles.css`; localhost-only Python bridge: `visual/server.py`.
- A manual `Spot player` command currently stands in for an Unreal AI sight stimulus; no real Unreal sensory component is connected.
- GOAP uses compact symbolic state, not a general combat simulator. It is **not** yet aware of environmental cover quality, projectile collision, or actual animation timings. The 2D tile line-of-sight model is only a prototype.
- The Hollow's posterior redistributes blocked-cell probability to the remaining prior belief; this is a deterministic state-update policy, not a fully physically modeled acoustic system.
- The Trooper still uses a 20×12 grid; when the player teleports, its goal changes and the graph is reconstructed. Incremental repairs are for cell/risk changes under a fixed goal.
- The Exposure Director times out *simulated wave tokens*; live NPC lifetime management belongs to the future Unreal integration.

## Regression checks

Use the existing CMake build, then:

```powershell
& "D:\MSYS2\ucrt64\bin\ctest.exe" --test-dir build-vscode --output-on-failure
```

The additional unit and functional tests validate lack of sound-based omniscience, GOAP retreat goal precedence, belief retention, reachable Hollow searches, hazard-aware routes, target-loss reactions and wave expiry. Keep the CI history and benchmark output as evidence in your viva. In Unreal, re-run all four simultaneously and profile frame time to satisfy the integration rubric.

## Next milestone (v0.4)

Record reproducible scenario traces and compare each algorithm against a documented simple baseline. Then implement the Unreal adapter and visual node/wire examples for Blueprint users before expanding the level.
