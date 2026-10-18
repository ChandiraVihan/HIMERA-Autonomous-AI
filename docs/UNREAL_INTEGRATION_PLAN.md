# Unreal Engine 5.8 integration plan (NOT implemented yet)

1. **Keep original player Blueprints unchanged initially.** Inspect `BP_FirstPersonCharacter`, current weapon firing events, and its Animation Blueprint before wiring NPC systems.
2. Create a new `HimeraAI` Unreal runtime module or plugin that compiles this standard C++ core using Unreal Build Tool. The standalone CMake project is **not** itself a `.uplugin`.
3. Build `USTRUCT(BlueprintType)` structures for observation and result types. Expose decision entry points using `UFUNCTION(BlueprintCallable)`. For stateful agents, use `UObject` wrappers or components that own planner instances.
4. Feed AI Perception (sight, hearing), game state (ammo, health, exposure) and world navigation information into the wrappers.
5. Security: connect action names to AI Controller move/attack/reload requests with animation state completion callbacks.
6. Hollow: gather sound events, convert observation positions to graph nodes, select candidate search location and navigate via AI Move To. Validate reachability.
7. Trooper: choose graph representation (room/door/waypoint), update blocked links dynamically, follow path as waypoints. Do **not** directly substitute grid movement for a NavMesh path request.
8. Director: own the instance in a level actor or world subsystem, track real cooldown elapsed time and dispatch spawn/post-process/audio effects to Blueprints.
9. Test in a small greybox `LV_AI_Test` with a `Nav Mesh Bounds Volume`, blocking collision, and placeholder NPC meshes before importing the chemical lab and forest.
10. Later move to your Blender laboratory, satellite dish and Fab forest. Record per-agent debug state and FPS with **all four active**.

For each Blueprint integration change, create a node/pin/wire diagram based on actual existing game Blueprints, rather than assuming their layout.
