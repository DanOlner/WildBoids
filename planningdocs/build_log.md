## Build log

Based on plan in @forward_plan.md

### Phase 0 — completed [16.2.26]

All of Steps 0.1–0.6 implemented in one pass. 28 tests, all passing.

**Decisions resolved:**
- **4 thrusters** as default (rear, left-rear, right-rear, front). The spec format supports any number.
- **Coordinate convention** locked in: +Y = forward, +X = right, CCW positive, radians. Documented in CLAUDE.md.
- **Energy deduction** deferred — the `energy` field exists on Boid but isn't decremented yet. Trivial to add when needed.
- **Catch2 v3.5.2** chosen as test framework.

**Files created:**

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Build config: C++20, Ninja, FetchContent (Catch2 + nlohmann/json), ASan+UBSan in Debug |
| `src/simulation/vec2.h` | 2D vector: arithmetic, rotation, cross product, normalisation |
| `src/simulation/rigid_body.h/.cpp` | Force/torque accumulation, semi-implicit Euler integration, drag |
| `src/simulation/thruster.h` | Body-local thruster → world-frame force + torque |
| `src/simulation/boid.h/.cpp` | Owns RigidBody + thrusters, steps physics |
| `src/simulation/world.h/.cpp` | Holds boids, steps sim, toroidal wrapping |
| `src/io/boid_spec.h/.cpp` | JSON load/save of boid specs, create Boid from spec |
| `data/simple_boid.json` | Standard 4-thruster prey boid |
| `tests/test_vec2.cpp` | 5 test cases: arithmetic, length, normalisation, rotation, cross product |
| `tests/test_rigid_body.cpp` | 4 test cases: rest, F=ma trajectory, torque, drag |
| `tests/test_thruster.cpp` | 5 test cases: force direction, zero power, torque sign, body rotation, cancellation |
| `tests/test_boid.cpp` | 4 test cases: forward motion, rotation, straight line, stationary |
| `tests/test_boid_spec.cpp` | 5 test cases: load, create, round-trip, invalid file, integration |
| `tests/test_world.cpp` | 4 test cases: physics step, X wrap, Y wrap, multiple boids |
| `.vscode/settings.json` | Ninja generator, clangd config |

**Thruster torque convention confirmed:** The left-rear thruster (position `{-0.3, -0.4}`, fires in `{1, 0}`) produces positive torque (CCW). This was verified by `test_thruster.cpp` — `cross2d({-0.3, -0.4}, {1, 0}) = 0.4`, so torque = 0.4 × 20 = 8.0 (positive = CCW). This means the left-rear thruster turns the boid to the left (CCW), which is correct: a thruster on the left side of the body firing rightward pushes the tail right, rotating the nose left.

**Notes for next phase:**
- The `Boid::step()` currently takes drag as parameters. When World calls it, it passes `config_.linear_drag` and `config_.angular_drag`. This works but means drag is a world property, not a per-boid property. May want to revisit if different boid types need different drag (e.g. streamlined predators vs bulky prey).
- `World::get_boids_mut()` exists for tests to poke at boid state. May want to remove or restrict later.
- The boid spec JSON uses `camelCase` keys (`momentOfInertia`, `maxThrust`) while C++ code uses `snake_case`. The JSON format should stay camelCase (it's a data format convention) — the translation happens in `boid_spec.cpp`.

### Phase 1 — completed [16.2.26]

SDL3 rendering working. Boids draw as oriented triangles, random wander behaviour produces lively scene, thruster indicators visible, fixed timestep loop smooth.

**Files created/modified:**

| File | Action | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | Modified | Added `find_package(SDL3)`, new `wildboids` GUI executable target |
| `src/display/renderer.h` | Created | Renderer class: SDL window, boid triangle drawing, thruster indicators |
| `src/display/renderer.cpp` | Created | SDL3 init, `SDL_RenderGeometry` triangles, `SDL_RenderLine` thrusters, world-to-screen with Y-flip |
| `src/display/app.h` | Created | App class: main loop, event handling, random wander |
| `src/display/app.cpp` | Created | Fixed timestep (120Hz), pause/resume, T to toggle thrusters |
| `src/main.cpp` | Created | Demo scene: 800×800 world, 30 prey boids, random positions/headings |

**Keyboard controls:**
- **Space** — pause/resume
- **T** — toggle thruster indicators
- **Escape** — quit

**Bug found and fixed — thruster torque mixed coordinate frames:**

The original `Thruster::torque()` crossed `local_position` (body frame) with `local_direction.rotated(body_angle)` (world frame). At `body_angle = 0` this worked by coincidence, but at other angles the torque was wrong — e.g. the centered rear thruster produced phantom torque instead of zero. The fix: compute `cross2d(local_position, local_direction)` with both vectors in body frame. The 2D scalar cross product is rotation-invariant, so `body_angle` isn't needed for torque at all. Visible symptom: boids built momentum only in the Y axis despite rotating.

**Decisions resolved:**
- World size (800×800) and window size (800×800 pixels) kept separate via scale factor in renderer — can change independently later.
- Boid triangle: 12 world units long, 8 wide (`BOID_LENGTH`, `BOID_HALF_WIDTH` constants in renderer).
- Thruster indicator lines: 40 world units max length, scaled by power. Orange-yellow colour.
- Dark background (RGB 10, 10, 15). Prey = green, predator = red.
- Random wander lives in `App::apply_random_wander()` — throwaway pre-brain behaviour, clearly temporary.

**28 tests still passing** (simulation library unchanged apart from torque fix).

### Phase 2 — completed [17.2.26]

Spatial grid and toroidal distance utility implemented. 42 tests, all passing (28 original + 5 toroidal + 9 spatial grid).

**Files created/modified:**

| File | Action | Purpose |
|------|--------|---------|
| `src/simulation/toroidal.h` | Created | `toroidal_delta()` and `toroidal_distance_sq()` — single source of truth for toroidal wrapping math |
| `src/simulation/spatial_grid.h` | Created | `SpatialGrid` class: uniform grid with insert, query, clear |
| `src/simulation/spatial_grid.cpp` | Created | Grid implementation: wrapped index lookup for toroidal queries |
| `src/simulation/world.h` | Modified | Added `SpatialGrid grid_` member, `grid()` accessor, `rebuild_grid()`, `grid_cell_size` to `WorldConfig` |
| `src/simulation/world.cpp` | Modified | Grid rebuilt at end of each `step()` call |
| `CMakeLists.txt` | Modified | Added `spatial_grid.cpp` to sim library, new test files |
| `tests/test_toroidal.cpp` | Created | 5 tests: within-range delta, X wrap, Y wrap, corner wrap, distance_sq consistency |
| `tests/test_spatial_grid.cpp` | Created | 9 tests: basic query, X/Y/corner wrapping, clear, non-toroidal, small radius, brute-force validation against 50 boids, multi-step consistency |

**Decisions resolved:**
- **Wrapped index lookup** (not ghost cells) — simpler to implement, good enough for current scale.
- **`vector<vector<int>>`** cell storage — simple, optimise later if profiling shows it matters.
- **Cell size configurable** via `WorldConfig::grid_cell_size` (default 100 world units). Will be set to `max(sensor ranges)` when sensors arrive.
- **Stores `int` indices** into the boids vector, not pointers — avoids iterator invalidation on vector reallocation.

**Brute-force validation test:** The key integration test spawns 50 boids at random positions, queries each with radius 150, and verifies that every boid found by brute-force all-pairs toroidal distance check is also returned by the grid query. This confirms no false negatives.

**Note:** `data/simple_boid.json` rear thruster `maxThrust` had been manually changed from 50 to 15 during Phase 1 renderer tweaking — restored to 50 to match the existing test expectations.

**Visual debug: neighbour lines [17.2.26]**

Added all-pairs neighbour line rendering to visually verify the spatial grid and toroidal wrapping. Press **D** to toggle. For each boid pair within `NEIGHBOUR_RADIUS` (100 world units), a blue line is drawn using `toroidal_delta` so connections across world edges take the short path. Confirms toroidal wrapping works correctly at boundaries — lines visibly connect boids across opposite edges. Maximum meaningful radius is `min(world_w, world_h) / 2` (400 for 800×800 world); beyond that, `toroidal_delta` can't distinguish directions. Files modified: `renderer.h/.cpp` (new `draw_neighbour_lines` method + toggle), `app.cpp` (D key handler).

### Phase 3 — completed [17.2.26]

Sensory system implemented. Each boid has an array of wedge-shaped sensor arcs that detect nearby boids and output normalised float signals. 56 tests, all passing (42 previous + 14 new).

**Files created/modified:**

| File | Action | Purpose |
|------|--------|---------|
| `src/simulation/sensor.h` | Created | `SensorSpec`, `EntityFilter`, `SignalType` enums, `angle_in_arc()` utility for ±π-safe arc containment check |
| `src/simulation/sensory_system.h` | Created | `SensorySystem` class: holds sensor specs, runs perception |
| `src/simulation/sensory_system.cpp` | Created | Arc intersection algorithm: grid query → distance check → body-frame angle → arc check → signal output |
| `src/simulation/boid.h` | Modified | Added `std::optional<SensorySystem> sensors` and `std::vector<float> sensor_outputs` |
| `src/simulation/world.h/.cpp` | Modified | Added `run_sensors()` step after grid rebuild in `World::step()` |
| `src/io/boid_spec.h` | Modified | Added `std::vector<SensorSpec> sensors` to `BoidSpec` |
| `src/io/boid_spec.cpp` | Modified | Parse/save sensor specs from JSON (degrees in JSON ↔ radians in C++), `create_boid_from_spec` wires up `SensorySystem` |
| `data/simple_boid.json` | Modified | Version bumped to 0.2, added 7 sensors: 5 forward arcs (36° each at 0°, ±36°, ±72°) + 2 rear arcs (90° each at ±135°) |
| `src/display/renderer.h/.cpp` | Modified | Sensor arc visualisation: wedge outlines (radial lines + arc curve), coloured by signal strength (dim cyan → bright yellow-green) |
| `src/display/app.cpp` | Modified | **S** key toggles sensor arc display |
| `CMakeLists.txt` | Modified | Added `sensory_system.cpp` to sim library, `test_sensor.cpp` to tests |
| `tests/test_sensor.cpp` | Created | 14 tests: `angle_in_arc` (center/edge/outside/wrap), perception (ahead/outside-arc/outside-range/self/toroidal-wrap/entity-filter/rotated-boid/sector-density/multi-sensor/world-integration) |
| `tests/test_boid_spec.cpp` | Modified | Updated version check to "0.2", added sensor count/angle/filter/signal-type checks, sensor round-trip verification |

**Keyboard controls now:** Space (pause), T (thrusters), D (neighbour lines), S (sensor arcs), Escape (quit).

**Decisions resolved:**
- **7 sensors** for the default boid: 5 forward-facing 36° arcs covering ±90° ahead, plus 2 wide 90° rear arcs. All `NearestDistance` signal type, `Any` entity filter, 100 world unit range. This gives 7 floats as neural network input.
- **Signal convention**: `1.0 = touching, 0.0 = nothing detected` (inverse distance normalisation). `SectorDensity` normalises by expected max of 10.
- **Sensor arc rendering**: Wedge outlines (two radial lines + segmented arc curve, 12 segments per arc). Drawn behind the boid triangle so the boid body stays visible on top.
- **JSON format**: Sensors use degrees (`centerAngleDeg`, `arcWidthDeg`) for human readability; C++ converts to radians on load. `filter` and `signalType` are optional string fields with sensible defaults.
- **Sensors are optional** on `Boid` via `std::optional<SensorySystem>` — boids without sensors (e.g. from old JSON specs) still work.
- **Perception runs after grid rebuild** in `World::step()`, giving sensors access to current-tick positions. One-tick delay: sensor outputs computed this tick will feed the brain next tick (Phase 4).

**Algorithm summary:** For each sensor on each boid: query spatial grid within `max_range` → for each candidate (skip self, apply entity filter) → compute `toroidal_delta` → distance check → rotate delta into body frame (`delta.rotated(-self.angle)`) → compute body-frame angle via `atan2(x, y)` (+Y = forward) → check against sensor arc via `angle_in_arc()` → accumulate nearest distance or count.

### Phase 4 — completed [17.2.26]

NEAT brain implemented end-to-end. Sensors → NEAT network → thrusters → physics pipeline fully working. 95 tests, all passing (56 from Phases 0–3 + 10 DirectWire + 9 genome + 11 network + 3 genome serialisation + 6 brain integration).

**Steps completed:**

- **4.1 — ProcessingNetwork + DirectWireNetwork**: Abstract `ProcessingNetwork` interface (`activate`, `reset`). `DirectWireNetwork` test fixture with fixed weight matrix + sigmoid activation. 10 tests.
- **4.2 — NEAT genome data types**: `NeatGenome` with `NodeGene` and `ConnectionGene`. `NeatGenome::minimal()` factory creates fully-connected input→output topology. Input nodes get `Linear` activation, output nodes get `Sigmoid`. 9 tests.
- **4.3 — NeatNetwork (phenotype from genotype)**: Feed-forward network built from genome. Kahn's algorithm for topological sort. Per-node incoming connection lists for correct multi-layer evaluation. 11 tests.
- **4.4 — Genome JSON serialisation**: Optional `genome` field in boid spec JSON. Nodes (id, type, activation, bias) and connections (innovation, source, target, weight, enabled) round-trip through JSON. 3 tests.
- **4.5 — Integration: brain-driven boids in World**: `Boid` gains `std::unique_ptr<ProcessingNetwork> brain`. `World::step()` calls `run_brains()` after sensors. `create_boid_from_spec()` builds `NeatNetwork` from genome if present. `apply_random_wander()` skips brain-driven boids. 6 tests.

**Files created/modified:**

| File | Action | Purpose |
|------|--------|---------|
| `src/brain/processing_network.h` | Created | Abstract interface: `activate(inputs, outputs)`, `reset()` |
| `src/brain/direct_wire_network.h/.cpp` | Created | Test fixture: fixed weight matrix, sigmoid, no evolution |
| `src/brain/neat_genome.h/.cpp` | Created | `NeatGenome`, `NodeGene`, `ConnectionGene`, `NeatGenome::minimal()` |
| `src/brain/neat_network.h/.cpp` | Created | Feed-forward NEAT network: topological sort, per-node evaluation, 4 activation functions |
| `src/simulation/boid.h` | Modified | Added `std::unique_ptr<ProcessingNetwork> brain` — makes `Boid` move-only (non-copyable) |
| `src/simulation/world.h/.cpp` | Modified | Added `run_brains()` step in pipeline after sensors |
| `src/io/boid_spec.h` | Modified | Added `std::optional<NeatGenome> genome` to `BoidSpec` |
| `src/io/boid_spec.cpp` | Modified | Genome JSON parse/save, `create_boid_from_spec()` builds `NeatNetwork` from genome |
| `src/display/app.cpp` | Modified | `apply_random_wander()` skips boids with a brain |
| `CMakeLists.txt` | Modified | Added brain `.cpp` files to sim library, new test files |
| `tests/test_direct_wire.cpp` | Created | 10 tests: zero/nonzero weights, saturation, bias, size mismatches, polymorphism |
| `tests/test_neat_genome.cpp` | Created | 9 tests: node count/types, full connectivity, innovation numbers, activations |
| `tests/test_neat_network.cpp` | Created | 11 tests: zero weights, saturation, DirectWire equivalence, hidden nodes (1, 2, diamond), disabled connections, bias, reset |
| `tests/test_boid_brain.cpp` | Created | 6 tests: sigmoid(0) power, world step activates brain, brainless boid unchanged, sensor→brain response, full pipeline forward motion, auto brain creation |
| `tests/test_boid_spec.cpp` | Modified | 3 new tests: no-genome fallback, genome field round-trip, network output equivalence after round-trip |
| `tests/test_sensor.cpp` | Modified | Replaced `std::vector{...}` initialiser lists with variadic `make_boids()` helper — `Boid` is now non-copyable due to `unique_ptr` |

**Key design note:** The pipeline order in `World::step()` is: physics → wrap → grid → sensors → brains. Brain outputs take effect on the *next* tick's physics step (one-tick delay), which is the intended design from the plan.

**Bug found and fixed — NeatNetwork hidden node propagation (Step 4.3):**

The initial implementation accumulated all connection sums in a single bulk pass before evaluating nodes. For hidden nodes, downstream nodes read `hidden.value` which was still 0.0 (not yet evaluated), so output was `sigmoid(0) = 0.5` regardless of input. Fix: changed to per-node evaluation in topological order using pre-built `incoming_` connection lists. Each node accumulates its inputs and applies its activation function before any downstream node reads its value.

**Structural notes for Phase 5:**

- `Boid` is now **move-only** (non-copyable) due to `std::unique_ptr<ProcessingNetwork> brain`. Code that previously used `std::vector{boid1, boid2}` initialiser lists must use `push_back`/`emplace_back` with `std::move` instead.
- The `ProcessingNetwork` interface is polymorphic — `DirectWireNetwork` and `NeatNetwork` are interchangeable. Phase 5 evolution will create `NeatNetwork` instances from mutated/crossed-over genomes.
- `NeatGenome::minimal(7, 4, next_innov)` produces 11 nodes and 28 connections with sequential innovation numbers starting from `next_innov`. The counter is passed by reference and updated.
- Genome JSON format: `"genome": { "nodes": [{id, type, activation, bias}...], "connections": [{innovation, source, target, weight, enabled}...] }`. Innovation numbers are preserved through serialisation (critical for crossover alignment).
- Four activation functions available: `Sigmoid` (output default), `Tanh`, `ReLU`, `Linear` (input default). Per-node, stored in `NodeGene`.
- Output nodes use `Sigmoid` → outputs naturally in [0, 1] → maps directly to thruster power [0, 1].

---

### Phase 5 build log — Evolution (Steps 5.1–5.3)

**Step 5.1: Innovation tracker + mutation operators** (95 → 117 tests)

Created the innovation tracking system and five NEAT mutation operators:

- **InnovationTracker** ([innovation_tracker.h](src/brain/innovation_tracker.h), [innovation_tracker.cpp](src/brain/innovation_tracker.cpp)): Per-generation cache mapping `(source_node, target_node)` → innovation number. When two genomes independently evolve the same structural mutation in the same generation, they get the same innovation number — critical for crossover alignment. Cache clears each generation via `new_generation()`.

- **Mutation operators** ([mutation.h](src/brain/mutation.h), [mutation.cpp](src/brain/mutation.cpp)):
  1. `mutate_weights` — perturb (Gaussian noise) or replace (uniform random) each connection weight
  2. `mutate_add_connection` — pick two random nodes, add a connection if one doesn't exist (respects topology: no connections *to* input nodes or *from* output nodes)
  3. `mutate_add_node` — split an existing connection: disable it, insert a hidden node, add source→new (weight 1.0) and new→target (original weight)
  4. `mutate_toggle_connection` — flip enabled/disabled on a random connection
  5. `mutate_delete_connection` — remove a connection and clean up any orphaned hidden nodes

Tests: [test_innovation.cpp](tests/test_innovation.cpp) (6 tests), [test_mutation.cpp](tests/test_mutation.cpp) (16 tests).

**Bug found and fixed — heap-use-after-free in `mutate_add_node` (ASan caught):**

The initial implementation held a reference to a vector element (`auto& conn = genome.connections[ci]`) then called `push_back()` on the same vector. The push_back reallocated the vector's storage, invalidating the reference. Subsequent reads of `conn.source`, `conn.target`, `conn.weight` were use-after-free. Fix: copy `source`, `target`, `weight` into local variables *before* any `push_back` calls. ASan (enabled in Debug builds) caught this immediately.

**Step 5.2: Crossover** (117 → 125 tests)

Created NEAT innovation-aligned crossover ([crossover.h](src/brain/crossover.h), [crossover.cpp](src/brain/crossover.cpp)):

- Index the other parent's connections by innovation number
- Iterate fitter parent's connections: **matching** genes randomly from either parent, **disjoint/excess** always from the fitter parent
- If a gene is disabled in either parent, 75% chance it stays disabled in offspring
- Collect needed nodes from the resulting connections plus all input/output nodes
- Build node map with fitter parent's node properties taking priority

Tests: [test_crossover.cpp](tests/test_crossover.cpp) (8 tests). All passed first try.

**Step 5.3: Speciation + Population management** (125 → 141 tests)

Created the speciation system and full population management:

- **Compatibility distance** ([speciation.h](src/brain/speciation.h), [speciation.cpp](src/brain/speciation.cpp)): δ = (c1 × E / N) + (c2 × D / N) + c3 × W̄ where E = excess genes, D = disjoint genes, W̄ = mean weight difference of matching genes, N = normalisation factor (genome size or 1 for small genomes).

- **Species assignment**: Each genome is compared to species representatives. If compatible (distance < threshold), it joins that species; otherwise a new species is created. Empty species are removed.

- **Population** ([population.h](src/brain/population.h), [population.cpp](src/brain/population.cpp)): Full generation cycle:
  1. **Evaluate** — run fitness function on all genomes, track per-species best fitness and stagnation
  2. **Advance generation** — remove stagnant species, compute adjusted fitness (fitness sharing: divide by species size), allocate offspring proportionally, apply elitism (best genome per species survives unchanged), trim to survival_rate for breeding, produce offspring via tournament selection (k=2) + crossover/mutation, re-speciate

Tests: [test_speciation.cpp](tests/test_speciation.cpp) (8 tests), [test_population.cpp](tests/test_population.cpp) (8 tests including XOR benchmark). All passed first try.

**XOR benchmark — validating that NEAT actually works:**

The XOR problem is NEAT's classic validation test ([test_population.cpp:153–242](tests/test_population.cpp#L153-L242)). XOR is the simplest problem that **cannot** be solved by a network with no hidden nodes — it's not linearly separable. A minimal NEAT genome (direct input→output connections only) can never solve XOR, no matter what weights it has. To solve it, the evolutionary process must:

1. **Discover** that a hidden node is needed (via `mutate_add_node`)
2. **Wire** it correctly (via `mutate_add_connection`)
3. **Tune** the weights (via `mutate_weights`)
4. **Protect** the innovation via speciation (so structural novelty isn't immediately outcompeted)

The test creates a population of 150 genomes with 3 inputs (2 + bias) → 1 output. Each generation, every genome is evaluated on the four XOR truth-table cases. Fitness = 4.0 − total_squared_error (perfect = 4.0, threshold for "solved" = 3.9). The test asserts that NEAT solves XOR within 200 generations, then verifies the winning network produces correct outputs (within 0.3 tolerance per case).

This benchmark validates that mutation, crossover, speciation, fitness sharing, and selection all work together correctly. With seed 42, it typically solves XOR in ~30–60 generations and runs in about 1 second.

**Phase 5 progress summary:**

| Step | Description | Status | Tests |
|------|-------------|--------|-------|
| 5.1 | Innovation tracker + mutation | Done | 22 |
| 5.2 | Crossover | Done | 8 |
| 5.3 | Speciation + population | Done | 16 |
| 5.4 | Food, energy, fitness evaluation | Done | 12 |
| 5.5 | Evolution loop integration | Done | 4 |

Total tests: 157 (up from 95 at end of Phase 4).

---

### Step 5.4: Food, energy, and prey fitness (141 → 153 tests)

Added food spawning/eating, energy mechanics (metabolism + thrust cost), death, and fitness tracking (`total_energy_gained`).

**Files modified:** `world.h/.cpp` (Food struct, WorldConfig food/energy params, spawn_food, check_food_eating, deduct_energy), `boid.h/.cpp` (alive flag, total_energy_gained, total_thrust()), `renderer.h/.cpp` (draw_food, skip dead boids), `app.cpp` (pass rng to world.step, skip dead boids in wander).

**Files created:** `tests/test_food.cpp` (12 tests: eating within/outside radius, toroidal eating, food spawning, metabolism, thrust cost, death, dead boid behavior, fitness tracking).

### Step 5.5: Evolution loop integration (153 → 157 tests)

Built the evaluation harness connecting `Population` (genome evolution) to `World` (physics simulation). The `run_generation()` function creates a World, spawns one boid per genome (with brain built from genome), pre-seeds food, runs N ticks, and returns each boid's `total_energy_gained` as fitness.

All boids run in the **same world** simultaneously — efficient and allows boid-boid sensor interactions. The `Population::evaluate()` receives cached fitness values from the world simulation.

**Files created:** [tests/test_evolution.cpp](tests/test_evolution.cpp) — 4 tests:

1. **Evaluation harness produces non-zero fitness** (0.8s) — Verifies the genome→boid→world→fitness pipeline works end-to-end. 10 genomes with perturbed weights, 2000 ticks. At least one boid finds food.

2. **Moving boids outperform stationary ones** (1.4s) — Validates the fitness gradient exists. Hand-crafted "mover" genome (strong rear thruster weights, sigmoid(3)≈0.95) vs "sitter" genome (all weights -5, sigmoid(-5)≈0.007). Movers sweep more area and find more food. This is the foundational gradient that evolution exploits.

3. **Fitness improves over generations** (22s) — The core evolution test. 20-genome population, 1200 ticks/gen, 20 generations. Verifies that NEAT weight evolution discovers genomes whose best fitness exceeds the generation-0 mean. The gradient is movement-based (sensors detect boids, not food yet), so the check is intentionally modest.

4. **All evolved genomes produce valid networks** (6s) — After 10 generations of mutation + crossover in the foraging context (with structural mutations enabled), every genome builds a valid `NeatNetwork` with outputs in [0, 1].

**Performance tuning:** Initial test parameters (50 pop × 3000 ticks × 30 gens) took 276 seconds. Reduced to (20 pop × 1200 ticks × 20 gens) for 22 seconds — same validation, 12× faster.

**Note on fitness gradient strength:** Current sensors detect other boids (`EntityFilter::Any`), not food. The fitness gradient comes purely from movement: boids that move forward sweep more area and encounter more food by chance. This is a weak but real signal — the "movers vs sitters" test confirms it clearly. For stronger directed foraging (turning toward food), food-specific sensors would need to be added to the sensory system. This is an enhancement for a future step.

### Step 5.5b: Food sensors (157 → 164 tests)

Added `EntityFilter::Food` so sensor arcs can detect nearby food items using the same arc/range mechanics as boid detection. This gives the brain a direct signal about food direction and distance, enabling evolution to discover sensor→steering correlations ("food on my left → fire right thruster").

**Implementation: Option A (brute-force food iteration).** For food sensors, iterate the entire food vector directly rather than using the spatial grid. The food list is small (60–100 items), making brute-force O(N_food) per sensor per boid trivially cheap. Same arc geometry (distance check, body-frame rotation, `angle_in_arc()`) reused for both food and boid detection.

**Sensor count change:** 7 → 10 sensors (7 boid + 3 food). Genome input count correspondingly 7 → 10. Output node IDs shift from 7–10 to 10–13. All test files updated to match.

**Food sensor layout in `simple_boid.json`:** 3 wide 120° arcs at 0°, −120°, +120° with 120 world unit range, covering full 360°. Fewer inputs than duplicating all 7 boid arcs — faster evolution.

**Files modified:**

| File | Change |
|------|--------|
| `src/simulation/sensor.h` | Added `Food` to `EntityFilter` enum |
| `src/simulation/sensory_system.h` | Added `const std::vector<Food>& food` parameter to `perceive()` and `evaluate_sensor()` |
| `src/simulation/sensory_system.cpp` | Refactored: extracted `check_arc()` and `compute_signal()` helpers. Food sensors brute-force iterate food vector; boid sensors use spatial grid. `passes_filter()` returns false for `EntityFilter::Food` |
| `src/simulation/world.cpp` | Pass `food_` to `perceive()` in `run_sensors()` |
| `src/io/boid_spec.cpp` | Added `"food"` parsing/serialization for `EntityFilter` |
| `data/simple_boid.json` | Added 3 food sensors (ids 7–9), version stays 0.2 |
| `src/main.cpp` | Derive genome input count from `spec.sensors.size()` instead of hardcoding 7 |
| `tests/test_sensor.cpp` | Updated all 9 existing `perceive()` calls to include food parameter; added 7 new food sensor tests |
| `tests/test_evolution.cpp` | Updated `make_prey_spec()` with 3 food sensors; all genome sizes 7→10; output node IDs shifted |
| `tests/test_boid_spec.cpp` | Sensor count checks 7→10; genome sizes 7→10; node indices shifted |
| `tests/test_boid_brain.cpp` | All genome sizes 7→10; output node IDs shifted |

**New tests (7):** Food detection within arc/range, food sensor ignores boids, boid sensor ignores food, food outside arc, food outside range, toroidal food detection across world edge, world step with food sensors.

**Build and test results:** Clean build (18/18 targets). 160 non-evolution tests in 4.66s, 4 evolution tests in 43.69s. All 164 tests passing.

**Phase 5 progress summary (updated):**

| Step | Description | Status | Tests |
|------|-------------|--------|-------|
| 5.1 | Innovation tracker + mutation | Done | 22 |
| 5.2 | Crossover | Done | 8 |
| 5.3 | Speciation + population | Done | 16 |
| 5.4 | Food, energy, fitness evaluation | Done | 12 |
| 5.5 | Evolution loop integration | Done | 4 |
| 5.5b | Food sensors | Done | 7 |
| 5.6 | Headless runner | Done | 3 |

Total tests: 167 (up from 164 at end of Step 5.5b).

---

### Step 5.6 build log — Headless evolution runner

**Files created:**
- `src/headless_main.cpp` — CLI evolution runner, no SDL dependency
- `data/sim_config.json` — Reference config with default world/NEAT/evolution parameters
- `tests/test_headless.cpp` — 3 integration tests

**Files modified:**
- `CMakeLists.txt` — Added `wildboids_headless` target (links `wildboids_sim` only, no SDL)

**Usage:**
```bash
./build/wildboids_headless [options]
```

**Options:**

| Flag | Default | Description |
|------|---------|-------------|
| `--generations N` | 100 | Number of NEAT generations to run |
| `--seed N` | 42 | RNG seed for reproducibility |
| `--population N` | 150 | Population size |
| `--ticks N` | 2400 | Simulation ticks per generation |
| `--spec PATH` | `data/simple_boid.json` | Boid spec JSON file |
| `--save-interval N` | 10 | Save champion genome every N generations |
| `--output-dir PATH` | `data/champions` | Directory for saved champion genomes |
| `--help` | — | Show usage |

**Output:**
- CSV stats to stdout: `gen,best_fitness,mean_fitness,species_count,pop_size`
- Summary to stderr (so you can redirect CSV to file: `./build/wildboids_headless > log.csv`)
- Champion genomes saved as JSON to output dir (e.g. `data/champions/champion_gen10.json`)

**Example:**
```bash
# Run 50 generations, save to file
./build/wildboids_headless --generations 50 --seed 42 > evolution_log.csv

# Smaller population, faster iterations
./build/wildboids_headless --generations 200 --population 50 --ticks 1200
```

**Tests added (3):**
1. Completes N generations without crash (population size and species count stable)
2. Saved champion genome loads from JSON and produces a valid network
3. Champion genome replays in a fresh world without crash

**Additional world config flags** (added after initial build):

| Flag | Default | Description |
|------|---------|-------------|
| `--world-size N` | 2000 | World width and height |
| `--food-max N` | 200 | Max food items |
| `--food-rate F` | 8 | Food spawns per second |
| `--food-energy F` | 15 | Energy per food item |
| `--metabolism F` | 0.1 | Energy drain per second |
| `--thrust-cost F` | 0.01 | Energy cost per thrust per second |
| `--save-best` | off | Save whenever a new all-time best fitness is found |

**Output details:**
- The final summary reports the all-time best fitness and which generation/file it came from:
  ```
  Evolution complete.
    Generations: 100
    All-time best fitness: 75 (gen 23)
    Best champion file: data/champions/champion_gen23.json
    Final gen species: 3
  ```
- "New all-time best" messages print to stderr as they happen during the run.
- Use `--save-best` to ensure the best champion is always saved (otherwise it may land between save intervals).

**Loading a champion into the GUI:**

The GUI app (`wildboids`) accepts `--champion` to load an evolved genome:

```bash
# Evolve headless, then watch the champion
./build-release/wildboids_headless --generations 100 --save-best
# The summary tells you which file has the best genome — use that:
./build-release/wildboids --champion data/champions/champion_gen23.json

# All boids share the same evolved brain; control count with --boids
./build-release/wildboids --champion data/champions/champion_gen23.json --boids 50
```

Without `--champion`, the GUI spawns boids with random NEAT weights (default behaviour).

**Release build** (no sanitizers, ~30x faster — use for evolution runs):
```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

---

### Step 5.7: Shared sim config + food source abstraction (170 → 182 tests)

Two changes in one step: (a) unified world config loading so headless runner and GUI share the same physics settings, and (b) a food source abstraction allowing switchable food spawning strategies via `sim_config.json`.

#### 5.7a: Shared sim config

Both `wildboids` (GUI) and `wildboids_headless` previously had hardcoded world defaults that diverged (different world sizes, food counts, metabolism, drag). Now both load from `data/sim_config.json` on startup via `load_sim_config()`, ensuring champions evolved headless replay with identical physics in the GUI.

**Files created:**
- `src/io/sim_config.h` — `SimConfig` struct wrapping `WorldConfig` + `PopulationParams` + evolution run params
- `src/io/sim_config.cpp` — JSON parser for `sim_config.json`, populates all config structs

**Files modified:**
- `src/headless_main.cpp` — Loads config from `--config` flag (default: `data/sim_config.json`), CLI flags override JSON values. Added `--angular-drag` and `--linear-drag` flags.
- `src/main.cpp` — Loads config from `--config` flag, window size matches world size from config
- `CMakeLists.txt` — Added `sim_config.cpp` to library
- `tests/test_sim_config.cpp` — Created: 6 tests (load, defaults, missing file, uniform/patch/default food mode parsing)
- `tests/test_boid_spec.cpp` — Changed hardcoded `max_thrust` check to `> 0` so tweaking `simple_boid.json` doesn't break tests

#### 5.7b: Food source abstraction

Extracted food spawning from `World::spawn_food()` into a `std::variant<UniformFoodSource, PatchFoodSource>` strategy. `World` still owns the food vector; the food source is a spawning strategy only. `check_food_eating()` and sensors are unchanged.

**Uniform mode** (default, backward compatible): Same as before — probabilistic spawning at random positions, capped at `max_food`.

**Patch mode**: N tightly clustered food sites (normal distribution around random centers). When total food drops by one patch's worth, a new patch spawns at a random location. This creates foraging pressure — boids must navigate to food clusters, and simple "drift forward" strategies are insufficient.

**Files created:**
- `src/simulation/food_source.h` — `UniformFoodConfig`, `PatchFoodConfig` config structs; `UniformFoodSource`, `PatchFoodSource` classes; `FoodSourceConfig` and `FoodSource` as `std::variant` aliases; `make_food_source()` factory
- `src/simulation/food_source.cpp` — Both implementations. Patch spawning uses `std::normal_distribution` around random centers with toroidal wrapping.
- `tests/test_food_source.cpp` — 9 tests: pre-seed counts, clustering validation, patch respawn on depletion, no over-spawn, partial depletion doesn't trigger, world integration, backward compat with flat config fields

**Files modified:**
- `src/simulation/world.h` — Added `FoodSourceConfig` to `WorldConfig`, `FoodSource food_source_` member to `World`, `pre_seed_food(rng)` public method
- `src/simulation/world.cpp` — Constructor syncs flat `WorldConfig` fields into `UniformFoodConfig` for backward compat with existing tests. `spawn_food()` delegates to food source via `std::visit`. Added `pre_seed_food()`.
- `src/io/sim_config.cpp` — Parses `"mode"` key in food section: `"uniform"` (default if absent) or `"patches"`
- `src/headless_main.cpp` — Replaced manual food pre-seed loop with `world.pre_seed_food(rng)`
- `CMakeLists.txt` — Added `food_source.cpp` to library, `test_food_source.cpp` to tests

**Config format:**

Uniform (backward compatible — no `mode` key needed):
```json
"food": { "spawnRate": 5.0, "max": 80, "eatRadius": 10.0, "energy": 15.0 }
```

Patches:
```json
"food": { "mode": "patches", "numPatches": 2, "foodPerPatch": 30, "patchRadius": 50.0, "eatRadius": 10.0, "energy": 15.0 }
```

**Backward compatibility:** Existing tests that set flat `WorldConfig` fields (`food_spawn_rate`, `food_max`, etc.) continue to work — the `World` constructor syncs these into a `UniformFoodConfig` when the variant is still the default.

**Phase 5 progress summary (updated):**

| Step | Description | Status | Tests |
|------|-------------|--------|-------|
| 5.1 | Innovation tracker + mutation | Done | 22 |
| 5.2 | Crossover | Done | 8 |
| 5.3 | Speciation + population | Done | 16 |
| 5.4 | Food, energy, fitness evaluation | Done | 12 |
| 5.5 | Evolution loop integration | Done | 4 |
| 5.5b | Food sensors | Done | 7 |
| 5.6 | Headless runner | Done | 3 |
| 5.7 | Shared config + food source abstraction | Done | 15 |

Total tests: 182.

---

### Phase 5b: Predator co-evolution (201 tests)

Added predator boids that catch and eat prey, with dual-population NEAT co-evolution. Predators and prey evolve in the same world simultaneously — predators are selected for catching prey, prey are selected for food foraging (and implicitly for evading predators).

**Step 5b.1: Predator spec + predation mechanics + dual evolution**

**Files created:**

| File | Purpose |
|------|---------|
| `data/simple_predator.json` | Predator boid spec: same 4 thrusters as prey, 7 `any` sensors + 3 `prey` sensors (replacing food sensors) + 1 speed sensor |
| `tests/test_predation.cpp` | 9 tests: catch within/outside radius, dead prey/predator, toroidal catch, predators don't eat food, prey don't catch prey, multi-catch, fitness tracking |
| `tests/test_dual_evolution.cpp` | 4 tests: dual fitness vector sizes, predators catch prey in simulation, two Population instances coexist, predator champion produces valid network |

**Files modified:**

| File | Change |
|------|--------|
| `src/simulation/world.h` | Added `predator_catch_radius` and `predator_catch_energy` to `WorldConfig`; added `check_predation()` private method |
| `src/simulation/world.cpp` | Implemented `check_predation()` (predator within catch_radius kills prey, gains energy); modified `check_food_eating()` to skip predator boids; added `check_predation()` call in `step()` pipeline |
| `src/io/sim_config.cpp` | Parse `"predator"` section from `sim_config.json` (catchRadius, catchEnergy) |
| `data/sim_config.json` | Added `"predator"` section with `catchRadius: 12.0` and `catchEnergy: 50.0` |
| `src/headless_main.cpp` | Full refactor for dual-population co-evolution: `run_generation()` accepts prey + predator genomes, spawns both types (prey at indices [0,N), predators at [N,N+M)), collects fitness separately. Evolution loop manages two `Population` instances. New CLI flags: `--predator-spec`, `--predator-population`. Backward compatible: without `--predator-spec`, runs prey-only as before. CSV output extended for co-evolution mode. Separate champion saving for predators. |
| `src/main.cpp` | Added `--prey-champion`, `--predator-champion`, `--predators N` CLI flags. Spawns predator boids alongside prey. `--champion` kept as alias for `--prey-champion`. |
| `CMakeLists.txt` | Added `test_predation.cpp` and `test_dual_evolution.cpp` to test target |

**Architecture decisions:**

- **Option A (flat boids vector):** Prey and predators share a single `boids_` vector in World. Prey are spawned at indices [0, prey_count), predators at [prey_count, total). The spatial grid, sensor system, and physics step all operate on the flat vector unchanged. Fitness collection slices by index range.
- **Predators don't eat food:** `check_food_eating()` skips boids with `type == "predator"`. Predator energy comes solely from catching prey.
- **Predation is O(predators × prey):** Simple brute-force distance check. No spatial grid needed for typical population sizes (30 × 150 = 4500 checks per tick).
- **Backward compatibility:** Headless runner without `--predator-spec` runs prey-only evolution unchanged. GUI without `--predators` spawns prey only.
- **Separate populations:** Prey and predator `Population` instances are fully independent with separate innovation trackers, species, and genomes.

**Headless runner usage:**

```bash
# Prey-only (backward compatible)
./build-release/wildboids_headless --generations 50 --save-best

# Co-evolution
./build-release/wildboids_headless --predator-spec data/simple_predator.json \
    --predator-population 30 --generations 100 --save-best

# Champions saved to data/champions/ (prey) and data/champions/predators/ (predators)
```

**GUI usage:**

```bash
# Prey + predators with random brains
./build-release/wildboids --predators 10

# With evolved champions
./build-release/wildboids --prey-champion data/champions/champion_prey_gen50.json \
    --predator-champion data/champions/predators/champion_predator_gen50.json
```

**201 tests, all passing** (182 previous + 9 predation + 4 dual evolution + 6 sim_config).

---

### Option K: Directional mouth (201 → 210 tests) [27.2.26]

Added a directional "mouth" mechanic so that eating (food or prey) requires the boid to face the target and be moving toward it. Previously both `check_food_eating()` and `check_predation()` were pure distance checks from the boid's center — any contact from any direction counted. The mouth adds two optional checks:

1. **Arc check** — target must fall within a forward-facing arc (reuses `angle_in_arc()` from the sensor system)
2. **Velocity dot check** — `dot(velocity, delta_to_target) > 0`, ensuring the boid is approaching, not reversing over the target

Both checks are gated by `config.mouth_enabled` (default `false`), so existing behavior is unchanged unless explicitly enabled.

**Files modified:**

| File | Change |
|------|--------|
| `src/simulation/world.h` | Added 3 fields to `WorldConfig`: `mouth_enabled`, `mouth_arc_width` (radians, default π = 180°), `mouth_require_approach` (default true) |
| `src/simulation/world.cpp` | Added `#include "simulation/sensor.h"` for `angle_in_arc()`. Refactored both `check_food_eating()` and `check_predation()` to compute `Vec2 delta` first (via `toroidal_delta` or subtraction), then derive `dist_sq` from it. Added mouth arc + velocity checks after the distance check passes. |
| `src/io/sim_config.cpp` | Parse `"mouth"` section from `sim_config.json` (`enabled`, `arcWidth` in degrees → radians, `requireApproach`) |
| `data/sim_config.json` | Added `"mouth"` section: `enabled: false`, `arcWidth: 180`, `requireApproach: true` |
| `tests/test_food.cpp` | 5 new tests: facing+moving eats, food behind not eaten, reversing doesn't eat, disabled reverts to omni, arc-only without approach check |
| `tests/test_predation.cpp` | 4 new tests: facing+approaching catches, prey behind not caught, reversing doesn't catch, disabled reverts to omni |

**Config format:**
```json
"mouth": {
    "enabled": false,
    "arcWidth": 180,
    "requireApproach": true
}
```

**Reused utilities:** `angle_in_arc()` from `sensor.h`, `Vec2::rotated()` for body-frame rotation, `Vec2::dot()` for velocity approach check, `toroidal_delta()` for directional delta.

**210 tests, all passing** (201 previous + 5 food mouth + 4 predation mouth).

---

### Option L: Compound-eye sensor refactoring (210 → 221 tests) [27.2.26]

Replaced the separate per-filter sensor arrays (7 "any" boid sensors + 3 food/prey sensors + 1 speed = 11 inputs) with unified "compound eyes" — 16 physical eyes each producing up to 3 channels (food, same-type, opposite-type) plus 1 speed sensor = **49 NEAT inputs**. Prey and predator specs are now identical except for the `type` field — "same" vs "opposite" is resolved at runtime from `boid.type`.

**Key design decisions:**

- **Multi-channel eyes**: Each eye detects food, same-type boids, and opposite-type boids simultaneously, providing correlated spatial information ("food AND predator at 2 o'clock")
- **Two-level channel system**: Channels appear in two places that serve different roles. The **boid spec** `"channels"` (in `simple_boid.json` / `simple_predator.json`) defines the NEAT network **structure** — which channel slots exist in the output array and their order. This is baked into the genome topology (16 eyes × 3 channels + 1 speed = 49 inputs). The **sim_config** `"channels"` (in `sim_config.json`) acts as a **runtime gate** — it populates `WorldConfig.enabled_channels`, and during `perceive_compound()` any channel not in that list produces 0.0 even though the NEAT input slot still exists. This lets you run experiments like food-only foraging (`"channels": ["food"]` in sim_config) without changing boid specs or retraining genomes — the boid still has 49 inputs, but the 32 same/opposite slots stay at zero
- **Same/Opposite naming**: Species-agnostic. A prey's "same" channel detects other prey; a predator's "same" channel detects other predators. Both use identical sensor specs
- **Variable arc width layout**: 5 narrow 18° eyes in front (high precision), 11 wider 27° eyes covering flanks and rear (coarser coverage), tiling the full 360°
- **Single grid query optimization**: Old system queried the spatial grid 7× per boid (once per boid sensor). New system queries once with max range across all eyes, then classifies each candidate
- **Backward compatibility**: Old champion files with `"sensors"` array still load via the legacy `SensorySystem` path. `4thruster_foodonly_simple_boid.json` untouched

**16-eye layout:**

| Region | Eyes | Center angles | Arc width |
|--------|------|--------------|-----------|
| Front | 0–4 | 0°, ±18°, ±36° | 18° each |
| Mid-flank | 5–8 | ±58.5°, ±81° | 27° each |
| Rear-flank | 9–12 | ±112.5°, ±139.5° | 27° each |
| Rear | 13–15 | ±166.5°, 180° | 27° each |

**New types added to `sensor.h`:**

```cpp
enum class SensorChannel { Food, Same, Opposite };

struct EyeSpec {
    int id = 0;
    float center_angle = 0;   // radians, body frame
    float arc_width = 0;      // radians
    float max_range = 0;      // world units
};

struct CompoundEyeConfig {
    std::vector<EyeSpec> eyes;
    std::vector<SensorChannel> channels;
    bool has_speed_sensor = true;
    int total_inputs() const;  // eyes.size() * channels.size() + speed
};
```

**Output layout:** `[eye0_food, eye0_same, eye0_opp, eye1_food, eye1_same, eye1_opp, ..., speed]`

**Files modified:**

| File | Change |
|------|--------|
| `src/simulation/sensor.h` | Added `SensorChannel` enum, `EyeSpec` struct, `CompoundEyeConfig` struct with `total_inputs()` |
| `src/simulation/sensory_system.h` | Dual-mode interface: legacy constructor + compound-eye constructor, `is_compound()`, `compound_config()`, `perceive_compound()` |
| `src/simulation/sensory_system.cpp` | Added compound-eye constructor, `input_count()` dispatch, `perceive()` dispatch, full `perceive_compound()` implementation with single grid query, channel gating via `config.enabled_channels` |
| `src/simulation/world.h` | Added `std::vector<SensorChannel> enabled_channels` to `WorldConfig` (defaults to all three) |
| `src/io/sim_config.cpp` | Parse `"sensors"` section with `"channels"` array |
| `src/io/boid_spec.h` | Added `std::optional<CompoundEyeConfig> compound_eyes` to `BoidSpec`, `sensor_input_count()` declaration |
| `src/io/boid_spec.cpp` | Dual-format load/save: `"compoundEyes"` key → compound path, else `"sensors"` → legacy. `create_boid_from_spec()` constructs appropriate `SensorySystem`. Added `sensor_input_count()` helper |
| `src/main.cpp` | Replaced `spec.sensors.size()` with `sensor_input_count(spec)` for NEAT genome creation |
| `src/headless_main.cpp` | Same `sensor_input_count()` update |
| `src/display/renderer.h` | Added `draw_one_arc()` private method |
| `src/display/renderer.cpp` | Refactored `draw_sensor_arcs()`: compound path iterates eyes with max-signal-across-channels colouring; legacy path uses extracted `draw_one_arc()` helper |
| `data/simple_boid.json` | Replaced `"sensors"` array with `"compoundEyes"` object (16 eyes, 3 channels, speed sensor). Type stays `"prey"` |
| `data/simple_predator.json` | Same compound-eye format as prey. Only `"type": "predator"` differs |
| `data/sim_config.json` | Added `"sensors": { "channels": ["food", "same", "opposite"] }` |
| `tests/test_sensor.cpp` | 9 new compound-eye tests: total_inputs, food/same/opposite channel detection, multi-eye output layout, disabled channels, arc filtering, speed sensor, toroidal detection |
| `tests/test_boid_spec.cpp` | Updated existing tests for compound-eye format (16 eyes, 49 inputs). Added 2 new tests: compound-eye round-trip, `sensor_input_count()` legacy vs compound |
| `tests/test_boid_brain.cpp` | Updated all `NeatGenome::minimal()` calls to use `sensor_input_count(spec)` instead of hardcoded 10. Updated output node index references (`nodes[n_inputs]` instead of `nodes[10]`) |

**New JSON spec format:**

```json
"compoundEyes": {
    "eyes": [
        {"id": 0, "centerAngleDeg": 0, "arcWidthDeg": 18, "maxRange": 100},
        ...
    ],
    "channels": ["food", "same", "opposite"],
    "speedSensor": true
}
```

**Backward compatibility verified:** Headless smoke test with `data/4thruster_foodonly_simple_boid.json` (legacy 11-sensor format) runs successfully. Existing dual-evolution tests with programmatic legacy specs (`make_prey_spec()`, `make_predator_spec()`) pass unchanged.

**221 tests, all passing** (210 previous + 9 compound-eye sensor + 2 compound-eye spec).
