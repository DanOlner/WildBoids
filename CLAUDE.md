# Wild Boids

Evolved predator/prey boid simulation. Originally Java (2007), now being rebuilt in C++.
Two boid types (predator, prey) with evolved sensory rules and thrust-based physics.
See spec.md and the planning docs listed there for full design context.

## Build

- C++20, Apple Clang, CMake + Ninja
- Build: `cmake --build build`
- Tests: `ctest --test-dir build --output-on-failure`
- Configure: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- Release build (no sanitizers, ~30x faster — use for evolution runs):
  `cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-release`
- Sanitizers (ASan + UBSan) are on in Debug builds
- Always run tests after implementing a step

## How to run

- GUI: `./build/wildboids [--champion data/champions/foo.json] [--boids 50] [--config data/sim_config.json]`
- GUI with predators: `./build/wildboids --prey-champion data/champions/prey.json --predator-champion data/champions/predators/pred.json --predators 10`
- Headless prey-only: `./build-release/wildboids_headless --generations 100 --save-best > log.csv`
- Headless co-evolution: `./build-release/wildboids_headless --predator-spec data/simple_predator.json --predator-population 30 --generations 100 --save-best`
- Config: `data/sim_config.json` (shared between GUI and headless — ensures evolved champions replay with identical physics)
- Without `--champion`, the GUI spawns boids with random NEAT weights

## Code style

- `snake_case` for functions and variables
- `CamelCase` for types/classes/structs
- Prefer value types and composition over inheritance
- `#pragma once` for include guards
- Keep headers light — use forward declarations, full includes in .cpp files

## Coordinate convention

- +Y = forward (boid's "front" direction in body frame)
- +X = right
- Positive angle = counter-clockwise (standard math convention)
- Angles in radians internally

## Architecture

- Simulation is a pure library with no graphics dependencies
- Layers communicate via float arrays: Sensors → Processing Network → Thrusters
- Each layer can be developed, tested, and swapped independently
- Renderer is a consumer of state, never a producer
- JSON (nlohmann/json) for boid specs and sim config
- Pipeline per tick: physics → wrap → grid → sensors → brains (brain outputs take effect next tick — one-tick delay)
- Boid is move-only (non-copyable) due to `unique_ptr<ProcessingNetwork>` brain
- Prey: 11 sensors (7 boid-detecting + 3 food-detecting + 1 speed) → NEAT network → 4 thrusters
- Predator: 11 sensors (7 boid-detecting + 3 prey-detecting + 1 speed) → NEAT network → 4 thrusters
- Output nodes use Sigmoid → [0,1] maps directly to thruster power
- Genome input count derived from `spec.sensors.size()`, not hardcoded

## Project structure

```
src/simulation/   — physics, boids, world, spatial grid, sensors, food sources
src/brain/        — ProcessingNetwork interface, NEAT genome/network, mutation, crossover, speciation, population
src/io/           — JSON loading (boid spec, sim config)
src/display/      — SDL3 renderer, app main loop (GUI only)
src/main.cpp      — GUI entry point
src/headless_main.cpp — CLI evolution runner (no SDL dependency)
tests/            — Catch2 tests, one file per component
data/             — simple_boid.json, simple_predator.json, sim_config.json, champions/
planningdocs/       — active planning docs (forward_plan.md, build_log.md, evolution_neuralnet_thrusters.md, etc.)
planningdocs/planning_archive/ — historical design docs, safe to ignore
```

## Conventions established in code

- Thruster torque: left-rear thruster (`{-0.3, -0.4}` firing `{1, 0}`) produces positive (CCW) torque — turns boid left. Right-rear produces negative (CW) torque — turns boid right.
- JSON spec files use `camelCase` keys; C++ code uses `snake_case`. Translation happens in `boid_spec.cpp`.
- Drag is currently a world-level property (passed into `Boid::step()`), not per-boid.
- Tests run from the build directory; `test_boid_spec.cpp` searches for `data/` in parent directories.
- Food sources are a `std::variant<UniformFoodSource, PatchFoodSource>` strategy, configured via `sim_config.json` `"mode"` key.
- Predation: predators within `catch_radius` kill prey and gain energy. Predators don't eat food. Configured via `sim_config.json` `"predator"` section.
- Dual-population co-evolution: headless runner manages two `Population` instances (prey + predator) evolving in the same world. Prey at boid indices [0,N), predators at [N,N+M).

## Current phase

Phase 5b (Predator co-evolution). 201 tests passing. Steps 5.1–5.7 + 5b.1 complete:
- 5.1–5.7: Core evolution (see build_log.md for details)
- 5b.1: Predator spec, predation mechanics, dual-population headless runner, GUI predator support

Next: Run co-evolution experiments, tune predator/prey balance (catch radius, energy, population ratios). See forward_plan.md Options A–H for other directions.

See planningdocs/forward_plan.md for the build plan and planningdocs/build_log.md for the build history.
See planningdocs/evolution_neuralnet_thrusters.md for NEAT design rationale and architecture decisions.
