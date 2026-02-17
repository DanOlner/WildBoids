# Step-by-Step Build Plan

## Where we are

We have extensive planning docs covering:

- **Physics**: 2D rigid body with thrust-based locomotion (boid_theory.md)
- **Architecture**: Sensors → Processing Network → Thrusters, communicating via float arrays (evolution_neuralnet_thrusters.md)
- **Platform**: C++ with SDL3 + ImGui for display, CMake + Ninja build, Apple Clang on macOS (plan_newplatform.md)
- **Data**: JSON for boid specs and sim config, nlohmann/json for serialisation (plan_newplatform.md)
- **Gotchas**: Java-to-C++ mental model guide (fromjava_gotchas.md)
- **Sim loop**: Physics → Collisions → Energy → Sensors → Brain → (store commands for next tick) (spec.md)

Nothing is built yet. No code exists.

---

## Phase 0: Minimal testable skeleton

**Goal**: Get a compiling C++ project with a build system, a test framework, the simplest possible boid, and a JSON loader that can read a boid spec file. No rendering, no evolution, no sensors, no neural net. Just physics.

This gives us:

1. A working build pipeline we can trust
2. A physics engine we can verify numerically
3. A JSON spec format we can iterate on from day one
4. A foundation everything else plugs into

### Step 0.1 — Project scaffold and build system

Create the directory structure and CMakeLists.txt:

```
wildboids/
├── CMakeLists.txt
├── src/
│   ├── simulation/
│   │   ├── vec2.h              ← 2D vector type (value type, inline)
│   │   ├── rigid_body.h        ← RigidBody struct (position, velocity, angle, angular_vel, mass, inertia)
│   │   ├── rigid_body.cpp      ← integrate(), applyForce(), applyTorque()
│   │   ├── thruster.h          ← Thruster struct (position, direction, max_thrust)
│   │   ├── boid.h              ← Boid struct: owns a RigidBody + vector<Thruster> + energy
│   │   ├── boid.cpp            ← Boid::step(dt): apply thruster forces to rigid body, integrate
│   │   ├── world.h             ← World: owns vector<Boid>, world dimensions, toroidal flag
│   │   └── world.cpp           ← World::step(dt): iterate boids, apply physics
│   └── io/
│       ├── boid_spec.h         ← BoidSpec struct: data-only description loaded from JSON
│       └── boid_spec.cpp       ← loadBoidSpec(path) → BoidSpec, createBoid(BoidSpec) → Boid
├── tests/
│   ├── test_vec2.cpp
│   ├── test_rigid_body.cpp
│   ├── test_thruster.cpp
│   ├── test_boid.cpp
│   └── test_boid_spec.cpp
├── data/
│   └── simple_boid.json        ← minimal boid spec (see below)
└── extern/
    └── (nlohmann/json, catch2 — fetched via CMake FetchContent)
```

CMake configuration:
- C++20, Ninja generator
- FetchContent for Catch2 and nlohmann/json (no manual dependency management)
- Sanitizers on in Debug builds (ASan + UBSan)
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clangd
- Separate `wildboids_sim` library target (no main, no graphics) and `wildboids_tests` test target

### Step 0.2 — Vec2 and RigidBody

**Vec2** — a simple value type:
- `x`, `y` (float)
- Operators: `+`, `-`, `*` (scalar), `/` (scalar)
- `dot()`, `length()`, `lengthSquared()`, `normalized()`
- `rotate(angle)` — rotate vector by angle in radians
- All inline in the header

**RigidBody** — the physical state of a boid:
```cpp
struct RigidBody {
    Vec2 position{0, 0};
    Vec2 velocity{0, 0};
    float angle = 0;          // heading in radians
    float angularVelocity = 0;
    float mass = 1.0f;
    float momentOfInertia = 1.0f;

    void applyForce(Vec2 force);          // accumulates into net force
    void applyTorque(float torque);       // accumulates into net torque
    void integrate(float dt, float linearDrag, float angularDrag);
    void clearForces();
};
```

Integration uses semi-implicit Euler (good enough, stable, simple):
```
velocity += (netForce / mass) * dt
velocity *= (1 - linearDrag * dt)
position += velocity * dt

angularVelocity += (netTorque / momentOfInertia) * dt
angularVelocity *= (1 - angularDrag * dt)
angle += angularVelocity * dt
```

**Tests for this step:**
- Vec2 arithmetic, rotation, normalisation
- RigidBody at rest stays at rest
- Apply a force for N ticks → verify position matches expected (F=ma, simple trajectory)
- Apply a torque → verify angle changes correctly
- Drag reduces velocity over time
- Zero-mass or zero-inertia edge cases (should we clamp? assert?)

### Step 0.3 — Thrusters

**Thruster** — a fixed actuator on the boid body:
```cpp
struct Thruster {
    Vec2 localPosition;     // offset from boid center of mass (body frame)
    Vec2 localDirection;    // direction it fires (body frame, normalised)
    float maxThrust;        // maximum force magnitude
    float power = 0;        // current power level [0, 1], set by brain (or test harness)
};
```

A thruster converts its power level into a world-frame force and torque on the rigid body:
```cpp
Vec2 worldForce = rotate(localDirection, body.angle) * (power * maxThrust);
float torque = cross2D(localPosition, rotate(localDirection, body.angle)) * (power * maxThrust);
```

Where `cross2D(a, b) = a.x * b.y - a.y * b.x` (the 2D cross product / perpendicular dot product).

**Tests:**
- Rear thruster (pointing forward from behind center) accelerates boid forward
- Left-rear thruster produces clockwise rotation (positive torque or negative depending on convention — nail down the convention here)
- Two equal opposite side thrusters cancel rotation, produce net forward force
- Thruster at power=0 produces no force
- Thruster at power=1 produces maxThrust force

### Step 0.4 — Boid (minimal)

**Boid** — owns a RigidBody and a set of Thrusters:
```cpp
struct Boid {
    RigidBody body;
    std::vector<Thruster> thrusters;
    float energy = 100.0f;

    // For now, no brain. Thruster power levels are set externally (by tests).
    void step(float dt, float linearDrag, float angularDrag);
};
```

`Boid::step()`:
1. For each thruster, compute world-frame force and torque, apply to `body`
2. Call `body.integrate(dt, linearDrag, angularDrag)`
3. `body.clearForces()`

**Tests:**
- Create a boid with a single rear thruster, set power=1, step N times → boid moves forward
- Create a boid with the full 4-thruster layout, fire only left-rear → boid rotates and curves
- Create a boid with left+right rear thrusters at equal power → boid moves straight forward
- Energy deduction per tick proportional to total thrust (if we add that now, or defer)

### Step 0.5 — Minimal boid spec (JSON) and loader

This is the first version of the boid spec file format. It describes a boid's *physical configuration* only — no sensors, no brain. Those come later and will extend this format.

**`data/simple_boid.json`:**
```json
{
    "version": "0.1",
    "type": "prey",
    "mass": 1.0,
    "momentOfInertia": 0.5,
    "initialEnergy": 100.0,
    "thrusters": [
        {
            "id": 0,
            "label": "rear",
            "positionX": 0.0,
            "positionY": -0.5,
            "directionX": 0.0,
            "directionY": 1.0,
            "maxThrust": 50.0
        },
        {
            "id": 1,
            "label": "left_rear",
            "positionX": -0.3,
            "positionY": -0.4,
            "directionX": 1.0,
            "directionY": 0.0,
            "maxThrust": 20.0
        },
        {
            "id": 2,
            "label": "right_rear",
            "positionX": 0.3,
            "positionY": -0.4,
            "directionX": -1.0,
            "directionY": 0.0,
            "maxThrust": 20.0
        },
        {
            "id": 3,
            "label": "front",
            "positionX": 0.0,
            "positionY": 0.5,
            "directionX": 0.0,
            "directionY": -1.0,
            "maxThrust": 15.0
        }
    ]
}
```

Convention: the boid's "forward" is +Y in body frame. The rear thruster sits behind the center (negative Y) and fires in the +Y direction (forward). This matches the diagrams in boid_theory.md.

**BoidSpec / loader:**
```cpp
struct ThrusterSpec {
    int id;
    std::string label;
    Vec2 position;
    Vec2 direction;
    float maxThrust;
};

struct BoidSpec {
    std::string version;
    std::string type;       // "prey" or "predator"
    float mass;
    float momentOfInertia;
    float initialEnergy;
    std::vector<ThrusterSpec> thrusters;
};

BoidSpec loadBoidSpec(const std::string& path);
Boid createBoidFromSpec(const BoidSpec& spec);
```

**Tests:**
- Load `simple_boid.json` → verify all fields parsed correctly
- `createBoidFromSpec()` produces a Boid with correct mass, inertia, thruster count
- Round-trip: create spec, save to JSON, reload, verify equality
- Invalid JSON (missing fields, wrong types) → clear error, not a crash
- Load the spec, create the boid, fire its rear thruster, step → boid moves forward (integration test connecting spec → physics)

### Step 0.6 — World (minimal)

**World** — contains boids and world parameters:
```cpp
struct WorldConfig {
    float width = 1000.0f;
    float height = 1000.0f;
    bool toroidal = true;
    float linearDrag = 0.05f;
    float angularDrag = 0.1f;
};

class World {
public:
    World(const WorldConfig& config);
    void addBoid(Boid boid);
    void step(float dt);

    const std::vector<Boid>& getBoids() const;
    const WorldConfig& getConfig() const;

private:
    WorldConfig config;
    std::vector<Boid> boids;

    void wrapPosition(Vec2& pos);  // toroidal wrapping
};
```

**Tests:**
- Add a boid, step the world → boid's physics update correctly
- Toroidal wrapping: boid at x=995 moving right at speed 10, after step, x wraps to ~5
- Toroidal wrapping for Y axis
- Wrapping disabled: boid can leave bounds (or clamp — decide convention)
- Multiple boids update independently

---

## What this gives us

After Phase 0, we can:

- **Load a boid from a JSON file** and put it in a world
- **Set thruster powers manually** (from test code) and watch the physics play out
- **Verify the physics numerically** — forces, torques, drag, integration, wrapping
- **Run everything headless** — no graphics needed, pure library + tests
- **Iterate on the spec format** — the JSON structure will grow as we add sensors and brains, but the physical base is established

## Phase overview

| Phase | What | Key deliverable |
|-------|------|-----------------|
| 1 | Simple rendering | SDL3 window, boid triangles, fixed timestep — **done** |
| 2 | Spatial grid | O(1) neighbour queries with toroidal wrapping — **done** |
| 3 | Sensory system | Sensor arcs → float array of signals per boid — **done** |
| 4 | NEAT brain | ProcessingNetwork interface → NEAT genome/network → brain-driven boids — **done** |
| 5 | Evolution loop | Mutation, crossover, speciation, food, prey foraging evolution |
| 5b | Co-evolution | Predator population, arms-race dynamics |
| 6 | Analytics & UI | ImGui controls, ImPlot graphs, data collection |

Each phase extends what exists without rewriting. The float-array interfaces between layers mean each phase plugs in cleanly.

---

## Phase 1: Simple rendering

**Goal**: Open an SDL3 window, draw boids as oriented triangles, run the simulation live with a fixed timestep, and see boids moving around a toroidal world. No ImGui yet, no sensors, no brain — just physics + display.

This gives us:

1. Visual confirmation that the physics is working (thrusters, rotation, drag, wrapping)
2. A debug tool for everything that follows (sensors, brains, evolution)
3. The display-layer architecture established early — clean separation from simulation

### Step 1.1 — SDL3 dependency and CMake changes

**Install SDL3** via Homebrew:
```bash
brew install sdl3
```

Homebrew is simpler than FetchContent for SDL3 — it's a large compiled library, not a header-only dependency. CMake finds it via `find_package`.

**CMakeLists.txt changes:**
- Add `find_package(SDL3 REQUIRED)`
- Create a new `wildboids` executable target (the GUI app) that links `wildboids_sim` + `SDL3::SDL3`
- The existing `wildboids_sim` library and `wildboids_tests` targets stay unchanged — no SDL dependency touches them
- The display code goes in `src/display/`

```
src/display/
├── renderer.h      ← Renderer class: owns SDL window + renderer, draws boids
├── renderer.cpp
└── app.h/.cpp      ← App class: main loop, event handling, fixed timestep
```

Updated target structure:
```
wildboids_sim    ← library, no graphics (unchanged)
wildboids_tests  ← tests, no graphics (unchanged)
wildboids        ← GUI executable: main.cpp + display/ + links wildboids_sim + SDL3
```

### Step 1.2 — Renderer (draw boids)

**Renderer** — owns the SDL window and renderer, draws the world state:

```cpp
class Renderer {
public:
    Renderer(int width, int height, const char* title);
    ~Renderer();  // cleans up SDL

    void draw(const World& world);
    void present();  // swap buffers

    bool should_close() const;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    void draw_boid(const Boid& boid, float scale);
    void draw_world_bounds(const WorldConfig& config, float scale);
};
```

**Drawing a boid as a triangle:**

Each boid is drawn as an isoceles triangle pointing in its heading direction. The triangle vertices in body-local space (where +Y = forward):

```
      tip (0, 0.6)           ← nose, pointing forward
       /\
      /  \
     /    \
    /______\
(-0.3, -0.4)  (0.3, -0.4)  ← rear corners
```

To draw in world space: rotate each vertex by `boid.body.angle`, translate by `boid.body.position`, scale to pixel coordinates.

Using `SDL_RenderGeometry()` with `SDL_Vertex` structs (position + colour). This is SDL3's triangle-based 2D rendering — efficient, no textures needed.

**Colour by type:**
- Prey: green
- Predator: red (for when we add predators later; for now everything is prey)

**Scale/viewport:** The world coordinates (0–1000) need mapping to the window (e.g. 800×800 pixels). A simple `world_to_screen()` function handles this. The scale factor = `window_size / world_size`. No camera panning yet — the whole world fits in the window.

### Step 1.3 — App (main loop with fixed timestep)

**App** — wires simulation and rendering together:

```cpp
class App {
public:
    App(World& world, Renderer& renderer);
    void run();  // blocking main loop

private:
    World& world_;
    Renderer& renderer_;
    bool running_ = true;
    bool paused_ = false;

    void handle_events();
};
```

**Fixed timestep loop** (from plan_newplatform.md):

```cpp
void App::run() {
    const double dt = 1.0 / 120.0;  // simulation at 120 Hz
    double accumulator = 0.0;
    Uint64 last_time = SDL_GetPerformanceCounter();
    double freq = SDL_GetPerformanceFrequency();

    while (running_) {
        Uint64 now = SDL_GetPerformanceCounter();
        double frame_time = (now - last_time) / freq;
        last_time = now;

        // Cap frame time to prevent spiral of death
        if (frame_time > 0.1) frame_time = 0.1;

        accumulator += frame_time;

        handle_events();

        if (!paused_) {
            while (accumulator >= dt) {
                world_.step(dt);
                accumulator -= dt;
            }
        }

        renderer_.draw(world_);
        renderer_.present();
    }
}
```

**Keyboard controls (minimal):**
- **Escape** or window close → quit
- **Space** → pause/resume simulation
- **R** → reset: clear all boids, respawn fresh

### Step 1.4 — main.cpp (demo scene)

**main.cpp** creates a world with some boids and runs the app:

```cpp
int main() {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.linear_drag = 0.02f;
    config.angular_drag = 0.05f;

    World world(config);

    // Load the boid spec and spawn some boids
    BoidSpec spec = load_boid_spec("data/simple_boid.json");

    // Spawn 30 boids at random positions with random headings
    std::mt19937 rng(42);
    for (int i = 0; i < 30; i++) {
        Boid boid = create_boid_from_spec(spec);
        boid.body.position = random_position(rng, config);
        boid.body.angle = random_angle(rng);
        // Give each boid some initial rear thrust so they move
        boid.thrusters[0].power = 0.3f + random_float(rng, 0, 0.4f);
        world.add_boid(std::move(boid));
    }

    Renderer renderer(800, 800, "Wild Boids");
    App app(world, renderer);
    app.run();

    return 0;
}
```

For now, boids have no brain, so we set their thruster power at spawn time and it stays constant. This gives us 30 boids cruising forward at varying speeds, wrapping around the torus. If we also give slight random side-thruster power, they'll curve — producing visible spirals and loops.

This is a "physics demo" scene. It proves:
- The renderer draws boids correctly (position, orientation, wrapping)
- The fixed timestep loop is smooth
- The simulation runs at the right speed
- Toroidal wrapping looks right visually

### Step 1.5 — Random wander behaviour (temporary, pre-brain)

To make the demo more interesting before we have brains, add a simple random wander that fires on each tick:

```cpp
// In World::step() or a dedicated function
void apply_random_wander(Boid& boid, std::mt19937& rng) {
    boid.thrusters[0].power = 0.4f;  // constant rear thrust
    // Random side thruster nudges
    float steer = std::uniform_real_distribution<float>(-0.3f, 0.3f)(rng);
    boid.thrusters[1].power = std::max(0.0f, steer);   // left
    boid.thrusters[2].power = std::max(0.0f, -steer);  // right
}
```

This is throwaway code — it gets replaced by the sensor→brain→thruster pipeline in Phase 4. But it means Phase 1 produces a lively scene of boids wandering around the world, which is much more satisfying to look at than static particles or straight lines.

**Important**: this goes in a clearly-labelled place (e.g. `src/simulation/demo_behaviors.h`) so it's obvious it's temporary and doesn't pollute the real architecture.

### Files to create/modify

| File | Action | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | Modify | Add `find_package(SDL3)`, new `wildboids` GUI target |
| `src/display/renderer.h` | Create | Renderer class: SDL window + boid drawing |
| `src/display/renderer.cpp` | Create | SDL init, triangle rendering, world-to-screen mapping |
| `src/display/app.h` | Create | App class: main loop, fixed timestep, event handling |
| `src/display/app.cpp` | Create | Main loop implementation |
| `src/main.cpp` | Create | Demo scene: spawn boids, run app |
| `src/simulation/demo_behaviors.h` | Create | Temporary random wander (clearly labelled as throwaway) |

### What we don't do in Phase 1

- **No ImGui** — that's Phase 6. Just SDL3 rendering for now.
- **No camera pan/zoom** — the whole world fits in the window. Add later if needed.
- **No per-boid selection or inspection** — just watch the swarm.
- **No sensor arc visualisation** — sensors don't exist yet.
- **No performance optimisation** — 30 boids with SDL_RenderGeometry is trivial.

### Decision points for Phase 1

1. **World size vs window size**: Should the world be 800×800 to match the window 1:1? Or keep world at 1000×1000 and scale? Recommendation: keep them separate from the start. World is 800×800 for now (easy mental mapping), but the renderer always uses a scale factor. This means we can change either independently later.

2. **Boid visual size**: How many pixels tall should a boid triangle be? Depends on world scale. If the world is 800 units and the window is 800 pixels, each unit = 1 pixel, so a boid triangle ~10–15 pixels tall (body length 10–15 in world units). This is adjustable — it's just a constant in the renderer.

3. **Background colour**: Black with coloured boids is the classic look. Or dark grey. Trivial to change.

4. **Thruster visualisation**: Should we draw small lines showing active thrusters? Would be useful for debugging physics. Recommendation: yes, as an optional toggle (press T to show/hide thrusters). Each active thruster draws a short line from its position in the fire direction, length proportional to power. Not essential but very helpful.

---

## Phase 2: Spatial grid

**Goal**: Build a uniform-grid spatial index so any system can answer "which boids are near position X?" in O(1). Also centralise toroidal distance into a single utility. This is the foundation for sensors (Phase 3) and collisions (Phase 5).

### Step 2.1 — Toroidal distance utility

Create `src/simulation/toroidal.h` with a single free function:

```cpp
Vec2 toroidal_delta(Vec2 from, Vec2 to, float world_w, float world_h);
```

Returns the shortest-path vector from `from` to `to` on the torus. This is the **only** place that knows about wrapping math — sensors, collisions, and all future systems call this instead of computing raw deltas.

Also add `float toroidal_distance_sq(...)` for fast range checks without sqrt.

**Tests:**
- Delta within half-world returns raw difference
- Delta across the X wrap boundary returns short path (positive and negative)
- Delta across the Y wrap boundary returns short path
- Corner case: delta across both axes simultaneously
- `toroidal_distance_sq` matches `toroidal_delta(...).length_squared()`

### Step 2.2 — Spatial grid (core)

Create `src/simulation/spatial_grid.h/.cpp`:

```cpp
class SpatialGrid {
public:
    SpatialGrid(float world_w, float world_h, float cell_size, bool toroidal);

    void clear();
    void insert(int boid_index, Vec2 position);
    void query(Vec2 pos, float radius,
               std::vector<int>& out_indices) const;

private:
    int cols_, rows_;
    float cell_size_;
    float world_w_, world_h_;
    bool toroidal_;
    std::vector<std::vector<int>> cells_;
};
```

**Key design points:**
- Stores `int` indices into `World::boids_`, **not** pointers (vector reallocation would invalidate them)
- Cell size should be >= maximum sensor range (placeholder: 100 units for now)
- Grid rebuilt from scratch every tick (simple, correct, fast for hundreds of boids)
- For toroidal worlds, query wraps cell indices: `((col % cols_) + cols_) % cols_`
- Callers do fine-grained distance checks on candidates — the grid provides broad-phase only

**Tests:**
- Insert N boids, query near each → correct neighbour set
- Query near world edge → boids on opposite edge returned (toroidal)
- Query at corner → finds boids across both axis wraps
- Query with small radius → no false negatives within range
- `clear()` + fresh insert → no stale data
- Non-toroidal mode → edge boids not returned across wrap

### Step 2.3 — Integrate grid into World

- `World` gains a `SpatialGrid` member, constructed from `WorldConfig`
- At the start of `World::step()`, rebuild: `grid_.clear()` then `grid_.insert(i, boids_[i].body.position)` for all boids
- Expose `const SpatialGrid& grid() const` for sensors to query
- Existing `World::wrap_position()` should call `toroidal_delta` or at least use consistent math

**Tests:**
- World with 50 boids: verify grid query returns same neighbours as brute-force all-pairs check
- Grid is consistent after multiple `World::step()` calls (no stale entries)

### Files to create/modify

| File | Action | Purpose |
|------|--------|---------|
| `src/simulation/toroidal.h` | Create | `toroidal_delta()`, `toroidal_distance_sq()` |
| `src/simulation/spatial_grid.h/.cpp` | Create | Uniform grid: insert, query, toroidal wrap |
| `src/simulation/world.h/.cpp` | Modify | Add grid member, rebuild each tick, expose accessor |
| `tests/test_toroidal.cpp` | Create | Toroidal distance tests |
| `tests/test_spatial_grid.cpp` | Create | Grid insert/query/wrap tests |

### Decision points for Phase 2

1. **Cell size**: Use `max_sensor_range` as cell size, or a fixed value? We don't have sensors yet, so pick 100 as placeholder and make it configurable. When sensors arrive, set it to `max(all sensor ranges)`.

2. **Ghost cells vs wrapped index lookup**: Ghost cells (extra ring of cells mirroring opposite edges) simplify queries but complicate insertion. Wrapped index lookup (`((q % cols) + cols) % cols`) is simpler to implement and good enough. Start with wrapped indices; switch to ghost cells only if profiling shows it matters.

3. **Flat vs nested storage**: `vector<vector<int>>` is simple but cache-unfriendly. Alternative: single flat buffer with a fixed max per cell, or a packed array with offsets. Start simple, optimise later if needed.

---

## Phase 3: Sensory system

**Goal**: Give each boid an array of sensors. Each sensor is a wedge-shaped arc that detects nearby boids and returns a normalised float signal. The sensor output array becomes the input to the neural network (Phase 4).

### Step 3.1 — Sensor spec and data types

Create `src/simulation/sensor.h`:

```cpp
enum class EntityFilter { Prey, Predator, Any };
enum class SignalType   { NearestDistance, SectorDensity };

struct SensorSpec {
    int id;
    float center_angle;   // offset from boid heading (body frame, radians)
    float arc_width;      // total angular width (radians)
    float max_range;      // detection radius (world units)
    EntityFilter filter;
    SignalType signal_type;
};
```

Extend `BoidSpec` and `data/simple_boid.json` to include a `"sensors"` array. JSON uses degrees; C++ converts to radians on load.

**Tests:**
- Load boid spec with sensors → verify count, angles, ranges parsed correctly
- Round-trip: save/load preserves sensor specs

### Step 3.2 — Sensory system (perceive)

Create `src/simulation/sensory_system.h/.cpp`:

```cpp
class SensorySystem {
public:
    explicit SensorySystem(std::vector<SensorSpec> specs);
    int input_count() const;

    void perceive(const std::vector<Boid>& boids,
                  const SpatialGrid& grid,
                  const WorldConfig& config,
                  int self_index,
                  float* outputs) const;

private:
    std::vector<SensorSpec> specs_;

    float evaluate_sensor(const SensorSpec& spec,
                          const std::vector<Boid>& boids,
                          const SpatialGrid& grid,
                          const WorldConfig& config,
                          const Boid& self) const;
};
```

**Arc intersection algorithm** for each sensor:
1. Query `grid.query(self.position, spec.max_range, candidates)`
2. For each candidate (skip self):
   - `delta = toroidal_delta(self.position, other.position, ...)`
   - Distance check: `delta.length_squared() <= max_range²`
   - Entity filter check
   - Rotate delta to body frame: `body_delta = delta.rotated(-self.body.angle)`
   - Angle in body frame: `atan2(body_delta.x, body_delta.y)` (+Y = forward convention)
   - Arc check: `|angle - center_angle|` within `arc_width / 2` (handle ±π wrap)
3. Signal output:
   - **NearestDistance**: `1.0 - (nearest_dist / max_range)`. 1.0 = touching, 0.0 = nothing detected.
   - **SectorDensity**: `count / expected_max` clamped to [0, 1].

**Tests:**
- Single boid directly ahead → NearestDistance returns correct normalised value
- Boid outside arc → signal is 0
- Boid outside range → signal is 0
- Self not detected
- Toroidal wrap: boid across world edge detected by sensor
- EntityFilter: prey sensor ignores predators
- Rotated boid: sensor arc moves with heading

### Step 3.3 — Integrate into Boid and World

- `Boid` gains `SensorySystem sensors` and `std::vector<float> sensor_outputs`
- `World::step()` runs sensor perception after grid rebuild, before brain activation
- One-tick delay: sensor outputs computed this tick feed the brain *next* tick

**Tests:**
- World with two boids: verify sensor outputs update each tick
- Paused boid: sensors still perceive (perception is passive)

### Step 3.4 — Sensor visualisation (renderer)

- Optional sensor arc drawing in `Renderer`, toggled by keypress (S)
- Draw wedge outlines for each sensor, coloured by signal strength
- Only for debugging; off by default

### Files to create/modify

| File | Action | Purpose |
|------|--------|---------|
| `src/simulation/sensor.h` | Create | `SensorSpec`, `EntityFilter`, `SignalType` enums |
| `src/simulation/sensory_system.h/.cpp` | Create | Arc intersection, signal computation |
| `src/simulation/boid.h` | Modify | Add sensors + sensor_outputs members |
| `src/simulation/world.cpp` | Modify | Add perceive step to `World::step()` |
| `src/io/boid_spec.h/.cpp` | Modify | Parse/save sensor specs from JSON |
| `data/simple_boid.json` | Modify | Add sensor array |
| `src/display/renderer.h/.cpp` | Modify | Optional sensor arc drawing |
| `tests/test_sensor.cpp` | Create | Sensor signal tests |
| `tests/test_sensory_system.cpp` | Create | Integration tests with grid + world |

### Decision points for Phase 3

1. **How many sensors for the default boid?** A reasonable starting set: 5 forward-facing arcs (covering ~180° ahead, 36° each) + 2 rear arcs (covering ~180° behind). Each with NearestDistance for prey. ~7 sensors = 7 floats input to brain.

2. **Signal normalisation convention**: `1.0 = close, 0.0 = nothing` is intuitive for "danger nearby" signals. Confirm this is the right polarity for the network.

3. **Sensor arc size in the renderer**: Small wedge outlines or filled wedges? Outlines are cheaper and less cluttered.

---

## Phase 4: NEAT brain

**Goal**: Build a feed-forward NEAT network that reads sensor outputs and writes thruster commands. This phase gets a *single* brain working end-to-end (sensors → network → thrusters). Phase 5 then gets a *population* of brains evolving. Testing them separately makes debugging tractable.

Brain code lives in `src/brain/` — a new directory, separate from `src/simulation/`. The brain layer communicates with sensors and thrusters only via float arrays, matching the loose-coupling architecture from [evolution_neuralnet_thrusters.md](evolution_neuralnet_thrusters.md).

### Step 4.1 — ProcessingNetwork interface + Direct-Wire smoke test

**What:** Define the `ProcessingNetwork` abstract interface and implement a trivial `DirectWireNetwork` that maps sensor outputs straight to thruster inputs with a fixed weight matrix. This is a test fixture (like `apply_random_wander()`), not a real brain — it proves the plumbing works before we add NEAT.

```cpp
// src/brain/processing_network.h
class ProcessingNetwork {
public:
    virtual ~ProcessingNetwork() = default;
    virtual void activate(const float* inputs, int n_in,
                          float* outputs, int n_out) = 0;
    virtual void reset() = 0;
};

// src/brain/direct_wire_network.h  (test fixture, not evolved)
class DirectWireNetwork : public ProcessingNetwork {
public:
    DirectWireNetwork(int n_in, int n_out);
    void set_weight(int in, int out, float w);
    void activate(const float* inputs, int n_in,
                  float* outputs, int n_out) override;
    void reset() override {}
private:
    int n_in_, n_out_;
    std::vector<float> weights_;  // n_in × n_out, row-major
    std::vector<float> biases_;   // n_out
};
```

**Tests:**
- Hand-set weights so that the forward sensor drives the rear thruster. Place two boids in a world, step the simulation, verify that the boid with a brain-driven thruster moves toward the detected boid. This is the first time sensors → brain → thrusters → physics runs end-to-end.
- Edge cases: more sensors than thrusters, fewer sensors than thrusters, all-zero inputs, all-one inputs.

**Replaces:** `apply_random_wander()` in `App` with brain-driven control for boids that have a `ProcessingNetwork`.

### Step 4.2 — NEAT genome data types

**What:** Define the genome representation — the genotype that encodes a network but *isn't* a network itself.

```cpp
// src/brain/neat_genome.h
enum class NodeType { Input, Output, Hidden };
enum class ActivationFn { Sigmoid, Tanh, ReLU, Linear };

struct NodeGene {
    int id;
    NodeType type;
    ActivationFn activation = ActivationFn::Sigmoid;
    float bias = 0.0f;
};

struct ConnectionGene {
    int innovation;
    int source, target;
    float weight = 0.0f;
    bool enabled = true;
};

struct NeatGenome {
    std::vector<NodeGene> nodes;
    std::vector<ConnectionGene> connections;

    // Create minimal topology: direct input→output, no hidden nodes
    static NeatGenome minimal(int n_inputs, int n_outputs,
                              int& next_innovation);
};
```

Start with static weights — no Hebbian plasticity. Modulatory neurons and ABCD learning rules can be added later as a genome extension without changing the core (see [evolution_neuralnet_thrusters.md](evolution_neuralnet_thrusters.md) Option C).

**Tests:**
- `NeatGenome::minimal(7, 4, ...)` produces 11 nodes (7 input + 4 output) and 28 connections (fully connected), each with a unique innovation number
- Nodes have correct types
- Connections link inputs to outputs only (no input→input, no output→output)

**No network activation yet** — this is pure data.

### Step 4.3 — Network activation (phenotype from genotype)

**What:** Build a `NeatNetwork` (the phenotype) from a `NeatGenome` (the genotype). This is the runtime network that actually computes `inputs → outputs`. It implements `ProcessingNetwork`.

```cpp
// src/brain/neat_network.h
class NeatNetwork : public ProcessingNetwork {
public:
    explicit NeatNetwork(const NeatGenome& genome);
    void activate(const float* inputs, int n_in,
                  float* outputs, int n_out) override;
    void reset() override;
private:
    // Nodes in topological evaluation order
    struct RuntimeNode { float bias; ActivationFn activation; float value = 0; };
    std::vector<RuntimeNode> nodes_;
    struct RuntimeConnection { int source, target; float weight; };
    std::vector<RuntimeConnection> connections_;
    std::vector<int> input_ids_, output_ids_;
    std::vector<int> eval_order_;  // topological sort of hidden + output nodes
};
```

The key algorithm is **topological sorting** of the network graph. For a minimal genome (no hidden nodes), this is trivial — outputs depend only on inputs. As hidden nodes are added later, the sort handles arbitrary DAGs. Recurrent connections (cycles) are detected and deferred to the next activation step.

**Tests:**
- Minimal genome (7→4): set all weights to 0 → outputs are sigmoid(0) = 0.5 for all thrusters
- Set one weight to a large positive value → corresponding output saturates toward 1.0
- Set one weight to a large negative value → corresponding output saturates toward 0.0
- Manually construct a genome with one hidden node → verify it activates correctly (input → hidden → output, two-layer propagation)
- Compare `NeatNetwork` output with `DirectWireNetwork` output on the same weight matrix — they should match for the minimal (no-hidden-node) case

**This is the moment NEAT-at-generation-0 becomes equivalent to DirectWireNetwork.** The smoke test from Step 4.1 and the NEAT network should produce identical results given identical weights.

### Step 4.4 — Genome JSON serialisation

Extend `src/io/boid_spec.h/.cpp`:
- `NeatGenome` serialises as `"genome": { "nodes": [...], "connections": [...] }` inside the boid spec
- Innovation numbers preserved (critical for crossover in Phase 5)
- This means a full boid spec JSON now has: physical props + thrusters + sensors + genome

**Tests:**
- Save boid with genome → load → network produces identical outputs
- Missing genome field → boid created with no network (graceful fallback)

### Step 4.5 — Integration — brain-driven boids in the World

**What:** Wire `NeatNetwork` into `Boid` so that `World::step()` runs the sense→think→act pipeline.

`Boid` gains `std::unique_ptr<ProcessingNetwork> brain`. The world step becomes:

```
1. Physics (existing)
2. Wrap positions (existing)
3. Rebuild grid (existing)
4. Run sensors (existing)
5. Run brains (NEW) — for each boid with a brain:
   brain->activate(sensor_outputs.data(), n_sensors,
                   thruster_commands.data(), n_thrusters);
   // Map commands [0,1] → thruster power [0,1]
   for (int i = 0; i < n_thrusters; i++)
       thrusters[i].power = thruster_commands[i];
```

**Tests:**
- Create a boid from `simple_boid.json`, attach a minimal `NeatGenome`, build its `NeatNetwork`. Step the world. Verify thrusters receive non-zero power (sigmoid(0) = 0.5 for all, since initial weights are 0).
- Hand-tune weights so the front sensor drives the rear thruster. Place a target boid ahead. Verify the boid accelerates toward it.
- Verify that boids *without* a brain (no `ProcessingNetwork`) still work — `apply_random_wander()` or no thrust. Backwards compatibility with the existing demo.

**This is the full pipeline working for the first time.** From here, everything is about making the brains *better* through evolution.

### Files to create/modify

| File | Action | Purpose |
|------|--------|---------|
| `src/brain/processing_network.h` | Create | Abstract interface: `activate(inputs, outputs)` |
| `src/brain/direct_wire_network.h/.cpp` | Create | Test fixture: fixed weight matrix, no evolution |
| `src/brain/neat_genome.h` | Create | `NeatGenome`, `NodeGene`, `ConnectionGene` structs |
| `src/brain/neat_network.h/.cpp` | Create | Feed-forward activation, topological sort, implements `ProcessingNetwork` |
| `src/simulation/boid.h` | Modify | Add `std::unique_ptr<ProcessingNetwork> brain` |
| `src/simulation/world.cpp` | Modify | Add brain activation step after sensors |
| `src/io/boid_spec.h/.cpp` | Modify | Genome JSON parse/save |
| `src/display/app.cpp` | Modify | Keep random wander as fallback for boids without brain |
| `CMakeLists.txt` | Modify | Add `src/brain/` files to sim library |
| `tests/test_direct_wire.cpp` | Create | DirectWireNetwork smoke test |
| `tests/test_neat_genome.cpp` | Create | Genome construction, minimal topology |
| `tests/test_neat_network.cpp` | Create | Activation correctness, hidden nodes, comparison with DirectWire |
| `tests/test_boid_brain.cpp` | Create | Integration: sensor → network → thruster → physics |

### Decision points for Phase 4

1. **Feed-forward vs recurrent**: Start feed-forward (DAG only). Recurrent NEAT (CTRNN) is more powerful but harder to implement. Can add later by allowing cycles in the topology and using a different activation strategy.

2. **Activation function per-node or global?** Per-node (stored in `NodeGene`). NEAT traditionally allows each node to have its own activation. Default to Sigmoid for output nodes (gives [0,1] for thruster power), Tanh for hidden nodes.

3. **Output clamping**: Use sigmoid on output nodes so outputs are naturally in [0,1] for thruster commands.

4. **Brain ownership**: `std::unique_ptr<ProcessingNetwork>` on `Boid` — polymorphic so `DirectWireNetwork` and `NeatNetwork` are interchangeable behind the same interface.

---

## Phase 5: Evolution loop

**Goal**: Implement the NEAT evolutionary cycle — mutation, crossover, speciation, food, energy, fitness — and run the first real evolutionary experiment: prey foraging for food.

The split from Phase 4 is deliberate: Phase 4 gets a *single* brain working. Phase 5 gets a *population* of brains evolving. These are different kinds of complexity — one is wiring, the other is population dynamics.

### Step 5.1 — Mutation operators

**What:** Implement the NEAT mutation functions that modify a genome.

| Operator | What It Does |
|----------|-------------|
| `mutate_weights(genome, rng)` | Perturb or replace connection weights (Gaussian noise) |
| `mutate_add_connection(genome, rng, innovation_tracker)` | Add a new connection between two unconnected nodes |
| `mutate_add_node(genome, rng, innovation_tracker)` | Split an existing connection, insert a hidden node |
| `mutate_toggle_connection(genome, rng)` | Enable/disable a random connection |
| `mutate_delete_connection(genome, rng)` | Remove a connection (pruning — not in original NEAT but we want it) |

The **innovation tracker** is a global counter + lookup table: "has the mutation source→target been seen before this generation?" If yes, reuse the innovation number. If no, assign a new one. This is essential for meaningful crossover.

```cpp
// src/brain/innovation_tracker.h
class InnovationTracker {
public:
    int get_or_create(int source_node, int target_node);
    void new_generation();

private:
    int next_innovation_ = 1;
    std::map<std::pair<int,int>, int> this_gen_cache_;
};
```

**Tests:**
- Innovation tracker: same pair → same innovation number; different pair → different number; after `new_generation()`, same pair gets a new number
- `mutate_weights`: verify weights change, verify they stay within bounds
- `mutate_add_connection`: verify a new connection appears with a new innovation number. Verify it doesn't create duplicates. Verify it works when the network is already fully connected (should be a no-op).
- `mutate_add_node`: verify the original connection is disabled, two new connections appear (source→new_node, new_node→target), the new node exists. Verify the new node has weight 1.0 on the incoming connection and the original weight on the outgoing connection (NEAT convention — preserves existing behavior).
- `mutate_toggle_connection`: verify enabled/disabled flips
- `mutate_delete_connection`: verify connection is removed. Verify orphaned nodes (no remaining connections) are cleaned up.
- Mutate a genome, build a `NeatNetwork` from the mutated genome, verify it still activates without crashing. (Structural validity.)

### Step 5.2 — Crossover

**What:** Combine two parent genomes into an offspring genome using NEAT's innovation-number alignment.

```cpp
NeatGenome crossover(const NeatGenome& fitter_parent,
                     const NeatGenome& other_parent,
                     std::mt19937& rng);
```

Matching genes (same innovation number): randomly pick from either parent.
Disjoint/excess genes: inherit from the fitter parent only.
Disabled in either parent → 75% chance disabled in offspring.

**Tests:**
- Two identical genomes → offspring is identical
- Parent A has connection [innovation=5] that Parent B lacks → offspring has it (A is fitter)
- Both parents have connection [innovation=3] → offspring gets it from one or the other (check over many trials that it's roughly 50/50)
- Crossover of genomes with different hidden nodes → offspring genome is structurally valid (no dangling connections, no duplicate innovations)
- Build `NeatNetwork` from crossover offspring → activates without crashing

### Step 5.3 — Speciation + Population management

**What:** The evolutionary loop that manages a population of genomes across generations.

Components:
- **Compatibility distance** δ between two genomes (count disjoint/excess genes + mean weight difference)
- **Species assignment**: group genomes into species by δ threshold
- **Fitness sharing**: divide each genome's fitness by its species size (prevents one species from dominating)
- **Selection**: tournament selection within species
- **Reproduction**: crossover + mutation → offspring. Elitism (best genome per species survives unchanged).
- **Generation cycle**: evaluate all → speciate → select → reproduce → repeat

Compatibility distance:
```
δ = (c1 * E / N) + (c2 * D / N) + c3 * W̄
```
Where E = excess genes, D = disjoint genes, W̄ = mean weight diff of matching genes, N = max genome size, and c₁, c₂, c₃ are tunable coefficients.

```cpp
// src/brain/population.h
class Population {
public:
    Population(NeatGenome seed, int pop_size, PopulationParams params);
    void evaluate(std::function<float(NeatGenome&)> fitness_fn);
    void advance_generation();
    const NeatGenome& best_genome() const;
    int generation() const;
    int species_count() const;
};
```

**Tests:**
- Create population of 50 from a minimal seed genome. Verify all are in one species initially.
- Assign random fitness. Advance one generation. Verify population size is preserved.
- Advance 10 generations. Verify species count > 1 (mutations should create diversity).
- Compatibility distance: identical genomes → δ = 0; genomes differing only in weights → δ = c3 * mean_weight_diff; genome with extra connections → δ includes excess/disjoint terms.
- **XOR benchmark** (NEAT's classic test): 2 inputs + 1 bias → 1 output. Fitness = accuracy on the 4 XOR cases. Verify that NEAT solves it within ~100 generations. This is the acid test — if our NEAT can't solve XOR, something is fundamentally wrong. (XOR requires at least one hidden node, so this tests structural mutation + speciation working together.)

### Step 5.4 — Food, energy, and prey fitness

**What:** Add food to the world and give prey boids a reason to move.

This is where the simulation stops being a physics demo and becomes an evolutionary system.

**Food mechanics:**
- Food is a set of points in the world, randomly placed
- When a prey boid's position is within `eat_radius` of a food point, it gains energy and the food is consumed
- New food spawns at a configurable rate (random positions) to maintain a target density
- Food appears on the renderer as small dots (simple — just coloured points)

```cpp
struct Food { Vec2 position; float energy_value = 10.0f; };

// In WorldConfig:
float food_spawn_rate = 2.0f;      // new food per second
int food_max = 100;                 // cap
float food_eat_radius = 8.0f;      // how close to eat
float food_energy = 10.0f;         // energy per food item

// World gains:
std::vector<Food> food_;
void spawn_food(std::mt19937& rng);
void check_food_eating();           // called each step
```

**Energy and fitness:**
- Prey boids start with `initial_energy` (from spec, currently 100)
- Each simulation step costs a small amount of energy (metabolism): `energy -= metabolism_rate * dt`
- Thruster usage costs energy proportional to thrust: `energy -= thrust_cost * power * dt`
- Eating food adds energy
- When energy reaches 0, the boid dies (removed from simulation or flagged inactive)
- **Fitness = total energy accumulated over lifetime** (not just survival time — rewards active foraging over sitting still)

**Tests:**
- Place food at a known position, place a boid nearby with a brain that fires the rear thruster. Step until the boid reaches the food. Verify energy increases.
- Verify food disappears when eaten and respawns.
- Verify boid with zero energy stops being active.
- Boid with high thrust runs out of energy faster.
- Run a population of 50 prey for one generation (6000 ticks). Verify fitness values are non-zero and vary across individuals. Verify the best-fitness boid found more food than the worst.

**Why food before predators?** Prey need a fitness signal to evolve *toward* something. Without food, fitness is just "survive longer" which rewards inactivity. With food, fitness rewards movement, sensor use, and navigation — exactly the behaviors we want the brain to discover. Once prey can forage, we have a working evolutionary system. Predators layer on top of that.

### Step 5.5 — Prey evolution — the first real test

**What:** Run the full evolutionary loop with prey boids foraging for food. No predators yet.

This is the first time we see whether the entire stack — sensors, NEAT brains, thrusters, physics, food, energy, fitness, speciation, mutation, crossover — produces emergent behavior.

**Setup:**
- 150 prey boids, all starting from the same minimal genome (7 sensors → 4 thrusters, no hidden nodes)
- World with toroidal wrapping, food spawning at steady state
- Each generation: reset world, spawn boids at random positions, run for 6000 ticks (~50 seconds at 120Hz), record fitness = total energy gathered
- Advance generation, repeat

**What to look for:**
- **Generation 0:** Boids move randomly (sigmoid(0) ≈ 0.5 on all thrusters, slight asymmetries from initial weight noise). Some stumble onto food by chance. Fitness is low and uniform.
- **Generation 10–30:** Weight evolution kicks in. Boids that happen to move forward (rear thruster > 0.5) find more food. Forward-moving genes spread.
- **Generation 30–100:** Sensor-thruster correlations emerge. Boids that turn toward detected food (front sensors driving differential steering) outcompete straight-line movers. Species with different foraging strategies appear.
- **Generation 100+:** If NEAT is working, hidden nodes appear in some genomes. These could enable "turn left when food is on the left AND nothing is ahead" — conditional logic that direct input→output mappings can't express.

**Test (automated):**
- Mean fitness increases over generations (not necessarily monotonically, but trending up)
- At least 2 species exist by generation 50
- Best-of-generation fitness at generation 100 is significantly higher than generation 0
- Best genome, when run in the renderer, visibly forages (moves, turns toward food, eats)

**This is the validation milestone.** If prey evolve to forage, the entire architecture works. Everything after this is enrichment.

### Step 5.5b — Food sensors (strengthening the foraging gradient)

**Problem:** The current sensors detect other boids (`EntityFilter::Any/Prey/Predator`) but not food. Without food sensors, the only fitness gradient is movement-based: boids that move forward sweep more world area and find food by chance. This is a weak signal — evolution can discover "move forward" but not "turn toward food". For directed foraging behaviour (the really interesting stuff), boids need to *see* food.

**Goal:** Add `EntityFilter::Food` so sensor arcs can detect nearby food items using the same arc/range mechanics as boid detection. This gives the brain a direct signal about food direction and distance, enabling evolution to discover sensor→steering correlations ("food on my left → fire right thruster").

**Design — extending the existing sensory system:**

The sensor arc geometry (center angle, arc width, max range, `angle_in_arc()`) is entity-agnostic — it just needs a position to test. The `EntityFilter` enum already selects what to detect. Adding food is a matter of:

1. **New enum value**: `EntityFilter::Food` (and optionally `EntityFilter::AnyEntity` for sensors that detect both boids and food, though `Any` currently means "any boid" so renaming may be needed for clarity).

2. **Pass food to perceive()**: `SensorySystem::perceive()` currently takes `const std::vector<Boid>&` and the spatial grid. Food sensors need `const std::vector<Food>&`. Two options:

   - **Option A — Widen the perceive() signature**: Add a `const std::vector<Food>& food` parameter. Food sensors iterate the food vector directly (no spatial grid needed — food doesn't move, and the list is small). Boid sensors continue using the spatial grid as before. Simple, no new data structures.

   - **Option B — Add food to the spatial grid**: Insert food items into the grid alongside boids. More efficient for large food counts, but requires distinguishing food indices from boid indices (e.g. negative indices, or a separate food grid). Over-engineering for 60–100 food items.

   **Recommendation: Option A.** The food vector is small (60–100 items). A brute-force distance check for food sensors is O(N_food) per sensor per boid — trivially cheap. No need for a food spatial grid until we have thousands of food items.

3. **Arc intersection for food**: Same algorithm as boid detection, but iterating `food` instead of `boids`:
   - For each food item: compute `toroidal_delta(self.position, food.position, ...)`
   - Distance check, body-frame rotation, `angle_in_arc()` — identical to boid sensors
   - Signal: `NearestDistance` = `1.0 - (nearest_food_dist / max_range)`, same convention

4. **Update boid spec**: Add food sensors to `simple_boid.json`. A natural layout: reuse the same 7 arc positions but with `"filter": "food"`. This doubles the sensor count to 14 (7 boid + 7 food) and the brain input size to 14.

   Alternatively, start with fewer food sensors — e.g. 3 wider arcs (left 120°, center 120°, right 120°) for a 10-sensor total (7 boid + 3 food). Fewer inputs = faster evolution.

5. **Update genome seed**: `NeatGenome::minimal(N_sensors, 4, ...)` — the number of input nodes changes to match the new sensor count. Existing genomes with 7 inputs won't be compatible, but since we're starting evolution fresh each run, this is fine.

**Changes required:**

| File | Change |
|------|--------|
| `src/simulation/sensor.h` | Add `Food` to `EntityFilter` enum |
| `src/simulation/sensory_system.h` | Add `const std::vector<Food>& food` parameter to `perceive()` |
| `src/simulation/sensory_system.cpp` | Handle `EntityFilter::Food` — iterate food vector with same arc logic |
| `src/simulation/world.cpp` | Pass `food_` to `perceive()` in `run_sensors()` |
| `src/io/boid_spec.cpp` | Parse `"food"` as valid filter value |
| `data/simple_boid.json` | Add food sensor entries |
| `tests/test_sensor.cpp` | New tests: food sensor detects nearby food, ignores boids, respects arc/range |
| `tests/test_evolution.cpp` | Update `make_prey_spec()` to include food sensors, adjust genome input count |

**Tests:**
- Food sensor detects food item within arc and range → signal > 0
- Food sensor ignores boids → signal = 0 when only boids nearby
- Boid sensor ignores food → signal unchanged when food is nearby
- Toroidal food detection across world edge
- Full pipeline: boid with food sensor + brain, food placed ahead, verify sensor fires and brain receives signal
- **Evolution gradient test (updated)**: With food sensors, directed foraging should emerge faster — movers-with-food-sensors should outperform movers-without-food-sensors

**Impact on evolution:** This is expected to dramatically strengthen the fitness gradient. Instead of evolution only discovering "move forward = find more food by chance", it can now discover "food signal on left → steer left". This is a direct sensor→actuator correlation that weight-only evolution (no hidden nodes needed) can find quickly. The Step 5.5 evolution test should show faster fitness improvement with food sensors enabled.

### Step 5.6 — Headless runner

Create a `wildboids_headless` CMake target (no SDL dependency) that runs evolution from command line:

```
./wildboids_headless --generations 100 --seed 42 --config data/sim_config.json
```

Outputs: generation stats to stdout, saves champion genomes to JSON periodically.

**Tests:**
- Headless runner completes N generations without crash
- Saved champion genome loads and produces a valid network

### Files to create/modify

| File | Action | Purpose |
|------|--------|---------|
| `src/brain/innovation_tracker.h/.cpp` | Create | Innovation number registry |
| `src/brain/mutation.h/.cpp` | Create | Weight perturbation, add connection, add node, toggle, delete |
| `src/brain/crossover.h/.cpp` | Create | Innovation-number-aligned genome crossover |
| `src/brain/speciation.h/.cpp` | Create | Compatibility distance, species assignment |
| `src/brain/population.h/.cpp` | Create | Population management, fitness sharing, generation cycle |
| `src/simulation/food.h` | Create | `Food` struct |
| `src/simulation/boid.h` | Modify | Add `fitness`, `alive` fields |
| `src/simulation/world.h/.cpp` | Modify | Add food spawning, eating, energy deduction |
| `src/display/renderer.h/.cpp` | Modify | Draw food as dots |
| `src/headless_main.cpp` | Create | CLI evolution runner |
| `CMakeLists.txt` | Modify | Add `wildboids_headless` target, new brain/sim files |
| `data/sim_config.json` | Create | Default evolution/sim parameters |
| `tests/test_innovation.cpp` | Create | Innovation tracker tests |
| `tests/test_mutation.cpp` | Create | Mutation operator tests |
| `tests/test_crossover.cpp` | Create | Crossover correctness tests |
| `tests/test_speciation.cpp` | Create | Compatibility distance, species assignment |
| `tests/test_population.cpp` | Create | Population management, generation cycle, XOR benchmark |
| `tests/test_food.cpp` | Create | Food spawning, eating, energy tests |

### Decision points for Phase 5

1. **Fixed vs variable generation length**: Fixed (6000 ticks) is simpler for data analysis. Dead boids stop simulating but their fitness is recorded.

2. **Population sizes**: Start with prey-only evolution (150 boids) to validate the pipeline. Add predators once prey evolve interesting behaviour.

3. **Elitism strategy**: Standard NEAT: 1 champion per species survives unchanged.

4. **Population parameters (starting values):**

| Parameter | Value | Notes |
|-----------|-------|-------|
| Population size | 150 | Per species |
| Species target | 8–15 | Adjust compatibility threshold to maintain |
| Survival rate | 25% | Top quarter of each species breeds |
| Elitism | 1 per species | Best organism always survives |
| Selection | Tournament, k=2 | Within species |
| Compatibility threshold (δ_t) | 3.0–6.0 | Tune based on speciation dynamics |

---

## Phase 5b: Co-evolution

**Goal**: Add a separate predator population that co-evolves alongside prey.

### Predator mechanics

- Predators have their own boid spec (potentially different sensors/thrusters, different brain)
- Predators gain energy by catching prey (position within `catch_radius`)
- Caught prey die (removed from that generation's simulation)
- Predator fitness = total prey caught (or total energy from prey)
- Predator metabolism is higher than prey (must hunt to survive)

### Co-evolution structure

- Two separate `Population` objects, each evolving independently
- Both share the same world during evaluation
- Prey fitness is affected by predators (dying early = less foraging time = lower fitness)
- Predator fitness is affected by prey behavior (smarter prey = harder to catch)
- This creates an arms race: prey evolve evasion, predators evolve pursuit, prey evolve better evasion...

### Tests

- Run with predators present. Verify prey mean fitness drops compared to no-predator runs (predation pressure exists).
- Verify predator mean fitness increases over generations (they get better at catching prey).
- Over many generations, verify that prey evolve visibly different behavior when predators are present vs absent (evasion vs pure foraging).

### When to add predators

After prey can be seen evolving navigational behaviour (turning toward/away from things). Predators need prey that actually move interestingly, or the co-evolutionary arms race has no starting point.

---

## Future phases: Plasticity, neuromodulation, and beyond

Once co-evolution is working:

- **Add plasticity parameters** (α, η, A, B, C, D) to `ConnectionGene`, initially all zero. Let mutation discover non-zero values. Compare fitness trajectories with and without plasticity enabled. See [evolution_neuralnet_thrusters.md](evolution_neuralnet_thrusters.md) Option C for full design.
- **Add neuromodulation** (modulatory node type). See if evolution discovers useful modulatory circuits.
- **Evolve sensor parameters** (currently fixed in JSON). Unlock center_angle, arc_width, max_range as evolvable genes. See if prey and predators evolve different sensory layouts.
- **Evolve thruster layout** (the "mutable" flag from boid_theory.md). See if body plans diverge.

Each of these is its own evolutionary experiment, built on the working foundation from Phases 4–5b.

---

## Phase 6: Analytics and ImGui (future — not yet detailed)

- ImGui overlay for live parameter tweaking
- ImPlot for fitness graphs, species counts, weight distributions
- DataCollector class sampling metrics at generation boundaries
- Population save/load for resuming runs
- Camera pan/zoom for large worlds

This phase will be planned in detail when Phases 4–5b are complete.

---

## Decision points for Phase 0

A few things to decide before or during implementation:

1. **3 vs 4 thrusters as default?** The spec file supports any number, so the format is fine either way. But the default `simple_boid.json` should represent our "standard" boid. Recommendation: start with 4 (including front brake), test both, decide later.

2. **Coordinate convention**: +Y = forward (up in body frame), +X = right. Positive angle = counter-clockwise. This matches standard math convention and the diagrams. Worth nailing down explicitly in code comments.

3. **Energy deduction**: include in Phase 0 or defer? It's simple (deduct proportional to total thrust per tick), but it's a gameplay mechanic more than physics. Could defer to Phase 1 or 2. Recommendation: include a basic version — it's trivial and tests the "boids can run out of energy" concept early.

4. **Test framework**: Catch2 (header-only, simple) vs Google Test (more features, better IDE integration). Recommendation: Catch2 — simpler setup, good enough, widely used.

---

## Build log

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
