# Wild Boids

Evolved predator/prey boid simulation. Originally Java (2007), now being rebuilt in C++.
Two boid types (predator, prey) with evolved sensory rules and thrust-based physics.
See spec.md and the planning docs listed there for full design context.

## Build

- C++20, Apple Clang, CMake + Ninja
- Build: `cmake --build build`
- Tests: `ctest --test-dir build --output-on-failure`
- Configure: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- Sanitizers (ASan + UBSan) are on in Debug builds
- Always run tests after implementing a step

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
- Renderer (when added) is a consumer of state, never a producer
- JSON (nlohmann/json) for boid specs and sim config

## Project structure

```
src/simulation/   — physics, boids, world (no graphics deps)
src/io/           — JSON loading/saving
tests/            — Catch2 tests, one file per component
data/             — JSON spec files
extern/           — dependencies fetched via CMake FetchContent
```

## Conventions established in code

- Thruster torque: left-rear thruster (`{-0.3, -0.4}` firing `{1, 0}`) produces positive (CCW) torque — turns boid left. Right-rear produces negative (CW) torque — turns boid right.
- JSON spec files use `camelCase` keys; C++ code uses `snake_case`. Translation happens in `boid_spec.cpp`.
- Drag is currently a world-level property (passed into `Boid::step()`), not per-boid.
- Tests run from the build directory; `test_boid_spec.cpp` searches for `data/` in parent directories.

## Current phase

Phase 0 complete (28 tests passing). Next: Phase 1 (rendering) or Phase 2 (spatial grid).
See step_by_step_planner.md for the full plan and build log.