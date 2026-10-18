# HIMERA Agent Algorithms: Model, Tests and Viva Preparation

## A. Facility Security Soldier — GOAP

**Problem:** A rigid patrol/chase/fire state machine cannot naturally plan around missing ammunition or unsafe exposure to fire.

Represent the world with symbolic predicates such as `KnowsTarget`, `UnderFire`, `InCover`, `HasAmmo`, and `Aimed`. Each action has positive/negative preconditions, add/remove effects and a cost. Search states using **Dijkstra's algorithm**, selecting the minimum accumulated action cost that achieves the goal. The objective itself depends on observed facts: suppress a known target, investigate a sound or patrol.

`cost(plan) = sum(action.baseCost + contextRiskPenalty)`

Example: `TakeCover -> Reload -> AcquireLineOfSight -> SuppressTarget`. Reloading while exposed carries extra cost, so the planner chooses a safer sequence. The planner returns both the ordered action names and the expanded symbolic-state count.

**Be prepared to explain:** GOAP is **goal/action planning**, not an alternative 3D pathfinder. Weighted cost functions change decisions. Dijkstra is used for plan search because action costs are nonnegative. Limitations include hardcoded available actions and a small world model, which must be extended with real enemy data.

## B. The Hollow — Probabilistic Belief Tracking

**Problem:** Chasing an exact player position through walls is not realistic or intelligent when the Hollow can hear sounds but cannot see the player.

Maintain a normalized probability distribution over walkable cells: `P(player at cell i)`. At each predicted movement step, retain `(1-m)` of probability mass at each cell and distribute `m` to reachable neighboring cells. After hearing a sound at `z`, use a Gaussian-shaped observation model:

`L(z|i) = (1-confidence) + confidence * exp(-||pos(i)-z||^2 / (2*sigma^2))`

`posterior(i) = prior(i) * L(z|i) / sum_j(prior(j)*L(z|j))`

Seeing the player yields a delta distribution; searching an empty region multiplies its likelihood by a small positive value. Select a search tile by posterior weight discounted by travel distance, danger and proximity to other Hollow search targets.

**Be prepared to explain:** This is a *discrete probabilistic filter*, not a trained machine-learning network and not a full sound simulation. Walls currently restrict belief movement but the acoustic likelihood is Euclidean; sound occlusion and reachability-aware target selection remain integration tasks.

## C. Containment Trooper — D* Lite

**Problem:** When doors lock or chemical hazards change traversal cost, replanning the entire shortest path from scratch may waste earlier search effort.

D* Lite maintains a previous-path search using `g(s)` and one-step lookahead `rhs(s)` values, and repairs inconsistencies after edge changes. The goal has `rhs(goal)=0`. Other cells use:

`rhs(s) = min_{s' in successors(s)} [ c(s,s') + g(s') ]`

The priority key is lexicographically ordered:

`key(s) = [ min(g,rhs) + h(start,s) + km, min(g,rhs) ]`

On this four-neighbor grid, use Manhattan distance as an admissible heuristic because each move costs at least 1. A directed edge from `s` into `s'` costs `1 + risk(s')` when both cells are walkable. After a door/hazard update, repair affected vertices instead of initializing an entirely new search. `moveStart` increments `km` to preserve incremental search consistency when the agent moves.

**Be prepared to explain:** D* Lite is suitable for **changing graphs**. It is not guaranteed faster than A* on a single static route, and it is not a magical navigation mesh. Compare with baseline replanning on the **same graph and changes**, reporting path cost, expanded vertices and wall-clock time.

## D. HIM-7 Exposure Director — Utility-Based Decision Making

**Problem:** Hallucination waves should feel reactive but avoid spawning unwinnable encounters when the player is low on resources.

Each candidate event gets an independently computed utility score based on continuous gameplay features (exposure, stress, health and ammunition). Hard constraints prevent events that violate safety conditions: low health, little ammunition, active-enemy cap, safe zones or recent wave cooldown. Select the **highest-scoring permitted event**.

Example `WaveUtility = 0.35 + 0.45*exposure + 0.15*health + 0.08*min(ammo,30)/30`.

An exposure wave is permitted only when `health >= .35`, `ammo >= 5`, `activeHollows < 4`, `secondsSinceWave >= 25`, `exposure >= .4`, and `!safeZone`.

**Be prepared to explain:** Utility AI is multi-objective scoring and arbitration, not GOAP and not pathfinding. Coefficients are *initial design choices*, not experimentally optimized numbers. Record playtest outcomes, add hysteresis/pacing, and tune them empirically.

## Validation scenarios

| Agent | Baseline | Advanced behavior to demonstrate | Edge case |
|---|---|---|---|
| Security | Always shoot on sight | Cover/reload/aim plan based on action costs | No ammo while under attack |
| Hollow | Exact-position chasing | Posterior search after sound; avoid rechecking empty cells | Obstructed paths and simultaneous Hollow |
| Trooper | Re-run full A* for each changed cell | Incremental D* Lite route repair | Door seals all routes then reopens |
| Director | Spawn on fixed timer | Exposure/resource-aware event selection | Low health, safe zone, active cap |

### Relevant academic limitations

The standalone simulation uses a **grid**, not Unreal's NavMesh. It also models enemies as algorithm controllers, not yet as animated skeletal characters. Integrated performance, accuracy and interaction behavior must be measured *inside the finished Unreal level* before making final submission claims.
