# Step-by-Step Build Plan

See @build_log for log of build of steps below.

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

## Where next? Options from here (added 24.2.26)

Phase 5 core is complete (182 tests, steps 5.1–5.7). Prey evolve nimble food foraging — they identify nearby food and steer toward it effectively. Champions from gen 45 show excellent foraging behavior. Some observed limitations worth addressing:

- **Boids struggle when far from food.** Once food is outside sensor range (~120 units), boids have no gradient to follow. They rely on stumbling into range or (interestingly) using boid-detection sensors as a proxy — following other boids that may be near food.
- **"Dead zone" behavior.** When all sensor inputs are near zero (nothing nearby), the network outputs are approximately sigmoid(bias), which produces a fixed thruster pattern. There's no evolved strategy for "explore when you see nothing."

The options below are roughly ordered from "most immediately impactful on evolution quality" to "longer-term enrichment." They're not strictly sequential — several could be pursued in parallel or in any order.

---

### Option A: Neural complexity cost (low effort, high impact)

**The problem it solves:** NEAT keeps adding hidden nodes that don't improve foraging. By gen 45, champions have 4 hidden nodes but aren't meaningfully better than gen 6 champions with none. Structural bloat without selection pressure against it.

**What to do:** Per-tick energy drain proportional to brain size (see "Neural Complexity Cost" section in evolution_neuralnet_thrusters.md). Enabled connections and hidden nodes cost energy each tick. A 4-hidden-node network needs to find ~2–3 extra food items per generation just to break even against a minimal network.

**Expected effect:** Network size stabilises at the minimum needed for the task. More complex food layouts (patches, scarcity) would naturally select for more complex brains, creating a task-driven complexity dial.

**Effort:** Small — add a few lines to `Boid::step()`, expose parameters in `sim_config.json`.

---

### Option B: Generational evolution in the GUI (medium effort, high impact)

**The problem it solves:** Evolution currently only runs in the headless runner or test harness. The GUI spawns boids with random weights and lets them run until they die — no reproduction, no selection, no visible evolution.

**What to do:** Option B from evolution_neuralnet_thrusters.md — timed generations in the live world. Run the simulation for N seconds of sim time, then trigger a generation boundary: collect fitness, advance the `Population`, respawn with new genomes. The existing `Population` machinery is used unchanged.

**What you'd see:** Boids getting visibly better at foraging across generations, in real time. Generation counter on screen, maybe a fitness graph (or just console output).

**Effort:** Medium — wire `Population` into `App`, add generation timer, handle world reset. The heavy machinery already exists.

---

### Option C: The "no input" problem — exploration behavior

**The problem it solves:** When sensors detect nothing, all inputs are 0. The network outputs sigmoid(sum_of_biases), producing a fixed thruster pattern. Boids with no nearby food or boids move in a fixed arc (or sit still) rather than exploring.

Several approaches, not mutually exclusive:

**C1: Bias input node.** Add a constant-1.0 input to the sensor array (a "bias sensor" that always fires). This is standard in NEAT — it gives the network a constant signal to work with, so output biases effectively become tunable even when all real sensors read zero. The minimal genome starts with connections from this bias node to all outputs. Evolution can tune these to produce a useful "default behavior" (e.g., gentle forward cruise with slight random drift).

**C2: Proprioception — internal state sensors.** This is broader than just "what to do when nothing's nearby." Proprioception gives boids awareness of their own body state — the biological equivalent of knowing where your limbs are, how fast you're moving, whether you're hungry. These signals are always available regardless of what's happening externally, which makes them useful for the dead-zone problem, but their value goes well beyond that. They create a richer input space for the brain to work with, enabling conditional strategies that depend on the boid's own state.

Candidate proprioceptive sensors:

- **Speed** (0–1, normalised by terminal velocity). The most immediately useful. A stationary boid knows it's stationary; a fast-moving boid can learn to brake before overshooting food. Also enables "cruise control" — evolution can discover a preferred speed and correct deviations. Currently boids have no idea how fast they're going; their thrust decisions are purely reactive to external stimuli.

- **Angular velocity** (-1 to 1, normalised). Tells the boid whether it's spinning. Could enable "if I'm spinning fast, stop steering" — anti-oscillation behavior. Also useful for smooth pursuit: a boid tracking food can sense its own turn rate and modulate steering to avoid overshooting. Complements speed — together they give a full picture of current motion state.

- **Heading change** (-1 to 1). How much the boid's heading changed since last tick. Subtly different from angular velocity — it's the *result* of angular velocity after drag, not the raw rate. Could be redundant with angular velocity, or could provide a cleaner signal for "am I currently turning?"

- **Energy level** (0–1, fraction of initial energy). Lets evolution discover energy-dependent strategies: "when energy is high, explore aggressively; when low, conserve" — or the reverse ("when low, take bigger risks to find food before dying"). This is hunger as a proprioceptive signal. With neural complexity cost (Option A), this becomes even more interesting — a boid with a costly brain needs to find food more urgently than a lean one, and can know this.

- **Thruster feedback** (0–1 per thruster, or a single aggregate). What am I currently doing with my thrusters? This sounds circular — the brain sets the thrusters, why does it need to know what it set? — but with the one-tick delay in the pipeline, the brain doesn't actually know its own previous output. Thruster feedback closes that loop. It's also the prerequisite for any kind of motor learning: you can't learn "that thrust pattern worked" without knowing what the pattern was. For a minimal version, a single "total thrust" signal (sum of all thruster powers / max possible thrust) might suffice.

- **Time alive** (0–1, fraction of generation length). Gives a sense of urgency — "the generation is nearly over, I should be more aggressive." In biology this would be something like circadian rhythm or seasonal awareness. Probably low priority but conceptually interesting.

**Impact beyond the dead-zone problem:** Proprioception fundamentally changes what the brain can learn. Without it, the network maps `external world state → thrust commands` — a pure stimulus-response system. With it, the mapping becomes `(external state, internal state) → thrust commands` — the boid can condition its behavior on what it's currently doing. This is the difference between a thermostat (reacts to temperature) and a driver (reacts to the road *and* their own speed/steering). Some specific behaviors proprioception enables:

- **Smooth pursuit**: Detect food ahead + detect own speed → modulate rear thrust to approach at controlled speed rather than slamming into food at full thrust then overshooting
- **Exploration spirals**: No external input + detect low speed → thrust forward; no external input + detect high angular velocity → reduce steering. This produces a natural expanding spiral search pattern
- **Energy triage**: Low energy + no food detected → reduce thrust to extend life; low energy + food detected → full thrust (worth the metabolic gamble)
- **Oscillation damping**: High angular velocity + food signal switching sides → reduce steering gain. This directly addresses the "wiggle" behavior seen in some evolved champions that detect food but can't steer smoothly toward it

**Recommendation:** Start with **speed** and **energy level** — these are the most immediately useful and easiest to implement (both are O(1) reads of existing boid state). Angular velocity is a strong third. Thruster feedback is interesting but adds 1–4 more inputs, which increases the genome size; defer unless the simpler proprioceptive sensors prove valuable. The others are speculative — implement if the first batch produces interesting results.

**C3: Random noise input.** Add a sensor that outputs uniform random noise each tick. This gives the network a source of stochasticity — even with identical real-sensor readings, different ticks produce different outputs. Evolution can learn to use this as a "jitter" signal for exploration. Biologically analogous to neural noise / stochastic foraging.

**Recommendation:** C1 (bias node) is nearly free and is standard NEAT practice — arguably an oversight that it's not already there. C2 (proprioception) is the richest vein — speed and energy sensors are high-impact and low-effort. C3 (noise) is interesting but harder for evolution to exploit.

**Effort:** Small per sensor — extend `simple_boid.json`, add evaluation in `sensory_system.cpp`. Bias node is trivial.

---

### Option D: Predator co-evolution (Phase 5b — large effort, the big goal)

**The problem it solves:** Prey foraging alone is a single-objective optimisation that plateaus. Predators create an arms race: prey evolve evasion, predators evolve pursuit, prey evolve better evasion. This is where the really interesting emergent behavior lives.

**What's needed:**
- Predator boid spec (own sensors, thrusters — could start identical to prey)
- Predator energy from catching prey (within `catch_radius`)
- Caught prey die (removed from simulation)
- Separate `Population` for predators, co-evolving alongside prey
- Predator fitness = total prey caught
- Higher predator metabolism (must hunt to survive)

**Prerequisites:** Prey should be reliably evolving navigational behavior first (they are). The "no input" problem (Option C) would be worth addressing first — predators that can't explore will never find prey.

**Effort:** Large — new boid type, separate population, modified world step, renderer changes. But the architecture is designed for it (separate populations sharing a world).

---

### Option E: Food source experiments (low effort, interesting results)

**The problem it solves:** Uniform random food is a simple environment. Patch-based food (already implemented) is more interesting but both are static strategies. Different food layouts create different selection pressures and may reveal whether current sensor/brain architecture is sufficient.

**Ideas:**
- **Moving food patches** — patches that drift slowly, requiring boids to track them over time (tests memory / persistent steering)
- **Seasonal cycling** — alternate between food-rich and food-poor periods. Selects for energy conservation strategies.
- **Gradient food** — food density highest in the center, lowest at edges (or vice versa). Tests whether boids can learn spatial preferences.
- **Food that runs away** — food items that flee nearby boids (proto-prey for predators, useful for evolving pursuit before adding real predators)

**Effort:** Low to medium — the `FoodSource` variant architecture supports new strategies cleanly.

---

### Option F: Replay and analysis tooling (medium effort, quality of life)

**The problem it solves:** Hard to understand what evolved brains are doing. Currently you can watch a champion in the GUI, but there's no way to inspect its network, compare generations, or replay specific moments.

**Ideas:**
- **Network topology visualisation** — draw the NEAT graph (nodes + connections, colored by weight sign/magnitude) in an ImGui panel. Small networks (10–25 nodes) are easy to lay out.
- **Fitness-over-generations graph** — even a simple console sparkline from the headless runner would help. ImPlot in the GUI would be better.
- **Champion replay gallery** — load multiple champions from different generations and watch them side-by-side in split viewports.
- **Sensor signal inspector** — click a boid, see its sensor values and thruster outputs as bar charts in real time.

**Effort:** Medium to large depending on scope. ImGui integration is Phase 6 in the plan but individual pieces (like console graphs) are quick.

---

### Option G: Recurrent connections (medium effort, speculative)

**The problem it solves:** Current networks are feed-forward — every tick is independent. A boid can't "remember" that it saw food to the left 5 ticks ago. Recurrent connections (a node's output feeding back to itself or a predecessor) provide a form of short-term memory.

**What NEAT already supports:** The genome representation allows cycles. The missing piece is the activation strategy — instead of topological sort (which requires a DAG), recurrent connections use the previous tick's activation value. This is a small change to `NeatNetwork::activate()`.

**Why it might help:** The "no input" problem is partly a memory problem. A boid that detected food 10 ticks ago should continue steering toward where it was, not instantly lose the signal. Recurrent connections could implement "I was recently heading toward food" persistence.

**Risk:** Recurrent networks can oscillate. May need damping or careful activation function choices. Also harder to debug and reason about.

**Effort:** Medium — modify network activation to handle cycles, allow NEAT mutations to create recurrent connections (currently add-connection only creates feed-forward ones).

---

### Suggested sequencing

For the most impactful progression:

1. **C1 (bias node)** — trivial, should have been there from the start
2. **C2 (speed sensor)** — gives boids self-awareness of movement state
3. **A (neural complexity cost)** — prevents bloat, creates efficiency pressure
4. **B (GUI evolution)** — makes evolution visible and interactive
5. **D (predators)** — the big milestone, transforms the project

Options E, F, and G can slot in anywhere as interest dictates. E (food experiments) is particularly good for validating whether the current architecture handles different selection pressures before committing to predators.

---

### Option H: Observation framework and steerable fitness (added 24.2.26)

This cuts across Options B (GUI evolution) and F (analysis tooling) but is a distinct architectural idea: a framework for **extracting structured observations from the simulation** and **using those observations to drive program behavior**, not just to display them. "Drive program behavior" is deliberately broad — observations should be a general-purpose event source that different systems can subscribe to. Shaping fitness is one use. Others: triggering a state save when an interesting threshold is crossed ("save the population when mean velocity alignment exceeds 0.5 for the first time"), halting a headless run early when fitness plateaus, logging a detailed snapshot when a boid achieves a new record, or switching food source strategy mid-run when the population reaches a certain competence level. 

The key architectural point is that observations are **computed once and available to many consumers** — the fitness system, the save system, the GUI, the logger — rather than each system re-deriving what it needs from raw world state.

Three capabilities, layered:

**(a) Observation layer — interrogating the simulation for emergent properties**

Currently fitness is a single number: `total_energy_gained`. But the interesting things happening in the simulation — flocking, dispersion patterns, pursuit behavior, correlated movement — are invisible to the evolutionary process. We can *see* them in the GUI but can't measure them, log them, or select for them.

An observation layer would compute derived metrics from world state each tick (or at intervals, or at generation boundaries). These are not sensor inputs to the brain — the boids don't perceive them. They're observations *about* the population for the experimenter.

**Candidate metrics:**

Spatial/collective:
- **Velocity alignment** — mean cosine similarity of velocity vectors across all (or nearby) boids. High values = flocking / correlated movement. Can be computed per-neighborhood using the spatial grid: for each boid, average alignment with its k nearest neighbors. A population-wide mean gives a single "flockiness" number per tick.
- **Spatial dispersion** — variance of inter-boid distances, or nearest-neighbor distance distribution. Low dispersion = clustering. High dispersion = spread out. The ratio of mean nearest-neighbor distance to expected-if-uniform tells you whether boids are clumping.
- **Group size distribution** — using the spatial grid, count connected components where boids are within some threshold distance. How many groups? How big is the largest? Do groups persist over time or form and dissolve? This is more expensive to compute but captures real flocking structure.
- **Territory fidelity** — do individual boids return to the same areas? Track each boid's position history (ring buffer of last N positions) and measure how much their range overlaps across time windows. High fidelity = territorial behavior, low = nomadic.
- **Movement efficiency** — food gathered per unit distance traveled. A boid that travels 5000 units and finds 3 food items is less efficient than one that travels 2000 units and finds 5. This separates "good at navigating to food" from "good at covering ground."

Individual behavioral:
- **Turn rate distribution** — histogram of angular velocity magnitudes across the population. Smooth foragers will cluster at low turn rates; erratic spinners at high rates. The shape of this distribution characterizes the population's movement style.
- **Speed distribution** — same idea. Are boids evolving a preferred cruising speed? Is there bimodality (some fast, some slow)?
- **Sensor-thruster correlation** — for each boid, compute the correlation between sensor inputs and thruster outputs over a time window. High correlation = reactive behavior. Low correlation = the brain is doing something more complex (or nothing useful). This directly measures whether the evolved network is actually using its sensors.
- **Food approach trajectories** — when a boid eats food, what did its path look like in the 50 ticks before eating? Straight approach? Spiral? Lucky collision? Classifying these trajectories tells you *how* the population is foraging, not just *how much*.

**Implementation sketch:**

```cpp
// src/simulation/observer.h

struct ObservationSnapshot {
    int tick;
    float mean_velocity_alignment;    // [-1, 1]
    float mean_nearest_neighbor_dist;
    float spatial_dispersion;         // variance of NN distances
    float mean_speed;
    float mean_angular_velocity;
    float food_eaten_this_interval;
    int alive_count;
    int group_count;                  // connected components within threshold
    float movement_efficiency;        // food per unit distance
    // ... extensible
};

class Observer {
public:
    // Register which metrics to compute (not all are cheap)
    void enable(ObservationMetric metric);

    // Called each tick (or every N ticks for expensive metrics)
    void observe(const World& world, int tick);

    // Retrieve accumulated data
    const std::vector<ObservationSnapshot>& snapshots() const;

    // Summary statistics over a generation
    ObservationSummary summarize(int from_tick, int to_tick) const;

    void clear();
};
```

The `Observer` sits *outside* the simulation — it reads `const World&` and produces data. It never modifies world state. This is critical: observations are a read-only lens, not a feedback loop into the physics. The simulation library stays pure.

Expensive metrics (group counting, trajectory classification) should be computed at lower frequency — every 100 ticks rather than every tick. Cheap metrics (mean speed, alignment) can run every tick with negligible cost for 150 boids.

For headless runs, the observer writes CSV or structured logs alongside the fitness CSV. For the GUI, observations feed into live displays — potentially just console output initially, ImPlot graphs later.

**(b) Observation-triggered actions — the observer as an event source**

The observer computes metrics. But metrics sitting in a vector are inert — something needs to *react* to them. Rather than hardwiring specific reactions, the observer should expose a simple event/callback interface that other systems subscribe to.

**Concrete examples of observation-triggered actions:**

- **Auto-save on milestones.** "Save the entire population state when mean velocity alignment first exceeds 0.4." You spot an interesting behavior emerging — you want to capture the genomes that produced it, not just the final champion. Currently the headless runner only saves the best-fitness genome; this would save the full population snapshot (all genomes, current species structure, the observation values that triggered the save).

- **Save on fitness plateau.** "If best fitness hasn't improved for 20 generations, save the population and log the observation state." Plateaus are interesting — they're the moments where NEAT is searching for structural innovations. Having the genome population from that moment lets you restart from the plateau with different parameters.

- **Conditional environment changes.** "When mean movement efficiency exceeds threshold X, switch from uniform food to patch food." This creates a curriculum — the environment gets harder as the population gets better. The food source variant architecture already supports swapping strategies; the observer provides the trigger signal.

- **Early stopping.** "If mean fitness hasn't improved in 50 generations AND species count has collapsed to 1, halt — this run is stuck." Saves compute on headless runs that have stalled.

- **Detailed snapshot logging.** "Every time a boid achieves a new all-time-best fitness, log its full trajectory (position history, sensor history, thruster history) for the last 200 ticks." This produces the data for Option F's "food approach trajectory" analysis without recording everything for every boid (which would be prohibitively expensive).

- **GUI notifications.** "Flash the generation counter when a new species appears." Low-priority but makes the GUI more informative.

**Design sketch — observer callbacks:**

```cpp
// Observer emits events that consumers subscribe to
using ObservationCallback = std::function<void(const ObservationSnapshot&, const World&)>;

class Observer {
public:
    // ... existing observe(), snapshots(), etc.

    // Threshold triggers — fire once when condition first becomes true
    void on_threshold(ObservationMetric metric, float threshold,
                      ThresholdDirection direction,  // Above or Below
                      ObservationCallback callback);

    // Periodic — fire every N observations
    void on_interval(int every_n_observations, ObservationCallback callback);

    // Plateau detection — fire when metric hasn't changed by more than
    // epsilon for N consecutive observations
    void on_plateau(ObservationMetric metric, int window, float epsilon,
                    ObservationCallback callback);
};
```

The callback receives `const ObservationSnapshot&` (the metrics) and `const World&` (the full state, for saving). It's a read-only interface — callbacks can log, save, and signal other systems, but can't modify the world. Environment changes (like switching food source) would go through a separate command queue that the main loop drains between ticks, keeping the observer→action path clean.

**Config-driven triggers (headless):**

```json
"triggers": [
    {
        "condition": { "metric": "velocity_alignment", "above": 0.4 },
        "action": "save_population",
        "once": true
    },
    {
        "condition": { "metric": "best_fitness", "plateau_generations": 20 },
        "action": "save_population"
    },
    {
        "condition": { "metric": "best_fitness", "plateau_generations": 50,
                       "and": { "metric": "species_count", "below": 2 } },
        "action": "halt"
    }
]
```

This is simple enough to implement without a full expression language — a flat list of conditions with optional `"and"` conjunctions. More complex logic can live in code rather than config.

**(c) Steerable fitness — user-defined evolutionary objectives**

This is the more ambitious part. Instead of `fitness = total_energy_gained`, allow the fitness function to be a weighted combination of survival fitness and observation-derived objectives. The experimenter (via config or GUI controls) decides what to optimize for.

**Why this matters:** Survival fitness alone selects for whatever works — which might be boring. A boid that sits near a food-rich patch and barely moves is fitter than an elegant swooping forager that covers more ground but wastes energy. By adding secondary objectives, you can steer evolution toward *interesting* behaviors without hand-designing them.

**Design — fitness as a weighted sum:**

```json
// In sim_config.json:
"fitness": {
    "components": [
        { "metric": "total_energy_gained", "weight": 1.0 },
        { "metric": "velocity_alignment_with_neighbors", "weight": 0.3 },
        { "metric": "movement_efficiency", "weight": 0.2 },
        { "metric": "distance_traveled", "weight": 0.1 }
    ],
    "normalization": "rank"
}
```

Each component is a metric that the observer computes per-boid (or per-boid from population statistics). The fitness function combines them:

```
fitness = Σ (weight_i × normalized_metric_i)
```

**Normalization matters.** Raw metric values have different scales — energy might range 0–500, alignment -1 to 1, distance 0–50000. Options:
- **Rank-based**: Convert each metric to a rank within the population (1 = best, N = worst), then weight the ranks. Simple, robust, scale-independent. This is essentially [NSGA-II](https://ieeexplore.ieee.org/document/996017)-style multi-objective handling simplified to a weighted sum.
- **Z-score**: Normalize each metric to mean 0, std 1 within the population. Sensitive to outliers but preserves magnitude differences.
- **Min-max**: Scale each to [0, 1] within the population. Simple but sensitive to extreme values.

Rank-based is probably the right default — it's what you'd want when mixing metrics with incompatible scales.

**Per-boid vs population-level metrics:** Some metrics are naturally per-boid (distance traveled, food eaten, movement efficiency). Others are population-level (mean alignment, spatial dispersion). For fitness, we need per-boid values. Population-level metrics can be decomposed:
- Velocity alignment → each boid's alignment with its neighbors (per-boid)
- Spatial dispersion → each boid's contribution to clustering (per-boid nearest-neighbor distance)
- Group membership → each boid's group size (per-boid)

**The flocking example in detail:** You observe that some evolution runs produce boids with correlated movement vectors and want to select for this. The setup:

1. Observer computes `velocity_alignment_with_neighbors` per boid each tick: average cosine similarity of this boid's velocity with all boids within 100 units. Range [-1, 1], higher = more aligned.
2. At generation end, each boid's mean alignment over its lifetime becomes a fitness component.
3. Config: `{ "metric": "velocity_alignment_with_neighbors", "weight": 0.5 }` alongside `{ "metric": "total_energy_gained", "weight": 1.0 }`.
4. Evolution now selects for boids that both eat well *and* move in the same direction as their neighbors.

The result should be recognizable flocking — but evolved, not hand-coded. The boids have to discover, through their sensor→brain→thruster pipeline, that matching neighbors' headings is rewarded. This is fundamentally different from Boids-algorithm flocking rules — the behavior is *evolved from neural architecture*, not *programmed*.

**Tension with "natural" evolution:** There's a philosophical question here. Adding velocity alignment to fitness is artificial selection — you're imposing a goal that doesn't emerge from the boid's ecological situation. A biologist might object: if flocking is adaptive, it should emerge from predation pressure (safety in numbers) without needing to be directly rewarded.

Both approaches are valid but answer different questions:
- **Natural fitness only** (energy gained, survival): "What behaviors emerge from the ecological pressures?" This is the simulation-as-experiment approach.
- **Steerable fitness**: "Can we evolve specific behaviors using this neural architecture?" This is the simulation-as-tool approach. It tests the *expressiveness* of the sensor→brain→thruster pipeline. If you can't evolve flocking even when directly rewarding it, the architecture has a gap.

The framework should support both. The default config has only `total_energy_gained` with weight 1.0 — pure natural selection. Users add secondary objectives when they want to explore the architecture's capability space.

**GUI interaction — live fitness steering:**

In the GUI (with Option B's generational evolution running), the experimenter could adjust fitness weights mid-run:
- Slider for each enabled metric's weight
- See the effect on evolved behavior within a few generations
- "What happens if I suddenly reward flocking at generation 50?"
- "What if I remove all survival pressure and only reward speed?"

This turns the GUI into an interactive evolution lab. It's not essential for the first implementation — config-file weights work for headless runs — but it's where the GUI version becomes qualitatively more powerful than headless.

**Multi-objective evolution (future extension):** The weighted-sum approach collapses multiple objectives into a scalar. True multi-objective optimization (Pareto frontiers, NSGA-II) would maintain a population that explores the tradeoff surface — some boids are great foragers but don't flock, others flock beautifully but find less food, and the algorithm preserves both. This is a natural extension but significantly more complex (requires replacing the single-species tournament selection with Pareto-dominance ranking). Worth noting as a possibility but not for v1.

**Implementation sequence:**

1. **Observer core** — `Observer` class, `ObservationSnapshot`, cheap metrics (mean speed, alignment, alive count). Wire into headless runner to produce CSV alongside fitness data. This is useful immediately for understanding evolution runs, independent of everything else.

2. **Observation triggers** — callback/threshold system on the observer. Config-driven triggers for headless runs (auto-save on milestones, early stopping on plateaus). This makes the observer *actionable*, not just informational. Population snapshot save/load is a prerequisite here.

3. **Per-boid metric tracking** — extend `Boid` or use a parallel vector to accumulate per-boid lifetime metrics (distance traveled, mean alignment, mean speed). These become available as fitness components.

4. **Configurable fitness function** — `FitnessConfig` in `sim_config.json`, fitness computed as weighted sum of per-boid metrics. Headless runner uses this. Replaces the hardcoded `total_energy_gained` in `Population::evaluate()`.

5. **GUI controls** — sliders for fitness weights, live metric display, trigger configuration. Requires Option B (GUI evolution) to be in place first.

**Files:**

| File | Action | Purpose |
|------|--------|---------|
| `src/simulation/observer.h/.cpp` | Create | Observation metrics computation |
| `src/simulation/boid.h` | Modify | Add per-boid metric accumulators (distance traveled, mean alignment, etc.) |
| `src/io/sim_config.h/.cpp` | Modify | Parse `"fitness"` config block |
| `src/headless_main.cpp` | Modify | Wire observer, output observation CSV |
| `src/display/app.cpp` | Modify | Wire observer for live display (later: GUI controls) |
| `tests/test_observer.cpp` | Create | Metric computation tests |

**Relationship to other options:** This subsumes parts of Option F (analysis tooling) — the observer *is* the analysis tool. It extends Option B (GUI evolution) by making fitness interactive. It's orthogonal to Options A, C, D, E, and G — those change what the boids can do; this changes how we measure and steer what they do.

**Effort:** Medium for the observer layer alone (step 1). Medium-large for the full steerable fitness pipeline (steps 1–3). Large if GUI controls are included (step 4). The observer on its own is valuable even without steerable fitness — just being able to plot "mean velocity alignment over generations" from a headless run would reveal whether flocking is emerging naturally.

---

### Option I: Tournament assembly — mixing evolved populations (medium effort, high experimental value)

**The problem it solves:** Currently, evolution runs are monolithic — one population evolves in isolation from start to finish. But the most interesting ecological questions involve interactions between populations that evolved under *different* conditions. For example:

- Evolve prey foragers for 100 generations with no predators. Then introduce predators. How quickly do prey adapt evasion on top of established foraging? Compare to prey that co-evolved with predators from the start — are the specialists or the generalists better?
- Evolve two prey populations independently with different food layouts (uniform vs patchy). Mix them in the same world. Which foraging strategy dominates? Do they coexist by exploiting different niches?
- Evolve predators against "easy" prey (weak foragers from early generations) to bootstrap pursuit behavior, then pit them against elite foragers. Curriculum learning for predators.

This is fundamentally about **composing evolutionary experiments** — treating evolved populations as building blocks that can be assembled into new scenarios.

**What we have already:**

The save/load infrastructure is in place:
- `save_boid_spec()` writes complete BoidSpec JSON (body + sensors + genome)
- `load_boid_spec()` reads them back
- `--champion` flag loads a single champion into the GUI
- `data/champions/` has 25+ saved champions from various runs
- Genomes are self-contained — a saved champion file has everything needed to recreate the boid

**What's missing:**

1. **Multi-population loading.** The GUI currently loads one champion file and clones it N times. We need to load *multiple different* champion files and spawn a mix. The headless runner similarly evaluates one population at a time.

2. **Tournament manifest file.** A JSON file that describes which populations to load, how many of each, and optionally which evolutionary parameters to use for each. This is the experiment definition.

3. **Mixed-population evolution.** When evolving with multiple pre-loaded populations, the evolutionary loop needs to handle them correctly — maintaining separate species pools, or merging them into a shared gene pool, depending on the experiment design.

**Tournament manifest format:**

```json
{
    "name": "prey_foragers_vs_fresh_predators",
    "description": "100-gen prey champions meet newly-seeded predators",
    "world_config": "data/sim_config.json",
    "populations": [
        {
            "role": "prey",
            "source": "data/champions/prey_gen100_elite.json",
            "count": 100,
            "evolve": true,
            "note": "Pre-evolved foragers, continue evolving under predation"
        },
        {
            "role": "predator",
            "source": "data/simple_predator.json",
            "count": 30,
            "evolve": true,
            "note": "Fresh predators with minimal genomes, evolve from scratch"
        }
    ]
}
```

Or for a mixed-prey experiment:

```json
{
    "name": "uniform_vs_patch_foragers",
    "populations": [
        {
            "role": "prey",
            "source": "data/champions/uniform_gen80.json",
            "count": 75,
            "evolve": true,
            "label": "uniform_specialists"
        },
        {
            "role": "prey",
            "source": "data/champions/patchy_gen80.json",
            "count": 75,
            "evolve": true,
            "label": "patch_specialists"
        }
    ]
}
```

**Key design decisions:**

1. **Separate vs merged gene pools.** When two prey populations are loaded, should they evolve as separate `Population` objects (preserving lineage, no crossover between them) or merge into a single population (allowing crossover, which may produce hybrids)? Both are scientifically interesting:
   - **Separate pools**: Tests which *strategy* wins over generations. Clean comparison. Each population's fitness trajectory is tracked independently.
   - **Merged pool**: Tests whether combining evolved strategies produces superior hybrids. Innovation numbers may conflict between independently evolved genomes — this needs careful handling (remap innovations on merge, or treat all cross-population genes as disjoint during crossover).

   Recommendation: **separate pools by default**, with an option to merge. Separate is simpler to implement (two `Population` objects sharing a world, which is already the plan for predator/prey co-evolution in Option D) and produces cleaner experimental results.

2. **Frozen vs evolving populations.** Sometimes you want one population to keep evolving while another stays fixed — e.g. "how do fresh predators evolve against prey that *don't* adapt?" The `"evolve": true/false` flag handles this. Frozen populations use the loaded genome unchanged across all generations; evolving populations go through the normal NEAT cycle.

3. **Population labeling.** When multiple populations share a role (two prey groups), they need labels for logging and analysis. The manifest's `"label"` field feeds into the observer (Option H) and fitness CSV output so you can track "uniform_specialists mean fitness" vs "patch_specialists mean fitness" across generations.

4. **Initial genome diversity.** Loading a single champion and cloning it 100 times gives zero genetic diversity — bad for evolution. Options:
   - **Load multiple champions** from different generations of the same run (e.g. gen 80, 85, 90, 95, 100). Each individual in the population gets one of these as its starting genome.
   - **Mutate on load**: Apply N rounds of weight mutation to each cloned genome to create a diverse starting population from a single champion. This is standard practice — "seed population from a champion with jitter."
   - **Load a saved population snapshot** (not just the best genome but all genomes from a generation). This requires saving full population state, which we don't currently do but would be valuable.

   The manifest could support all three via the `"source"` field:
   ```json
   "source": "data/champions/prey_gen100.json"           // single champion, mutate to diversify
   "source": ["data/champions/prey_gen80.json",           // array of champions
               "data/champions/prey_gen90.json",
               "data/champions/prey_gen100.json"]
   "source": "data/populations/prey_run42_gen100.pop"     // full population snapshot (future)
   ```

**CLI integration:**

```bash
# Headless tournament
./build-release/wildboids_headless --tournament data/tournaments/prey_vs_predator.json \
    --generations 100 --save-best

# GUI tournament
./build/wildboids --tournament data/tournaments/prey_vs_predator.json
```

The `--tournament` flag replaces `--champion` (which loads a single type). When a tournament manifest is provided, boid spawning follows the manifest instead of the default single-spec path.

**Relationship to other options:**

- **Option D (predator co-evolution):** Tournament assembly is a *superset* of basic co-evolution. Co-evolution is "prey + predators evolving together from scratch." Tournament assembly is "any mix of pre-evolved or fresh populations in any combination." Implementing D first gives the two-population infrastructure; tournament assembly generalizes it.
- **Option H (observations):** Population labels from the manifest feed directly into the observer — per-population fitness tracking, per-population behavioral metrics. "Are the uniform specialists clustering more than the patch specialists?"
- **Option B (GUI evolution):** The GUI needs to display which populations are present and let the user inspect them separately — e.g. click a boid and see "uniform_specialist, gen 12 (since tournament start), original lineage: prey_gen80."

**Implementation sequence:**

1. **Tournament manifest parser** — load the JSON, validate populations, resolve source files. Small.
2. **Multi-source spawning** — modify `main.cpp` and `headless_main.cpp` to spawn from manifest instead of single spec. Includes "mutate on load" for diversity. Medium.
3. **Multi-population evolution** — support separate `Population` objects per manifest entry with `"evolve": true`. Wire into `run_generation()`. This overlaps heavily with Option D's two-population infrastructure. Medium.
4. **Per-population logging** — label-aware CSV output, per-population fitness tracking. Small.
5. **Population snapshot save/load** — save all genomes + species structure, not just the champion. Enables full-population seeding. Medium.

**Files:**

| File | Action | Purpose |
|------|--------|---------|
| `src/io/tournament.h/.cpp` | Create | Tournament manifest parser |
| `src/io/boid_spec.h/.cpp` | Modify | Support loading arrays of specs |
| `src/main.cpp` | Modify | `--tournament` flag, multi-population spawning |
| `src/headless_main.cpp` | Modify | Tournament mode evolution with separate populations |
| `src/brain/population.h/.cpp` | Modify | Population snapshot save/load |
| `data/tournaments/` | Create | Directory for tournament manifests |
| `tests/test_tournament.cpp` | Create | Manifest parsing, multi-population spawning |

**Effort:** Medium. The hardest part is multi-population evolution (step 3), but this overlaps with Option D's infrastructure. The manifest parser and multi-source spawning are straightforward. The payoff is high — this turns the project from "run evolution" into "design evolution experiments."

---

### Option J: Multi-seed batch runs — searching the evolutionary landscape (low-medium effort, high experimental value)

**The problem it solves:** NEAT evolution is stochastic. The same configuration with seed 42 might produce elegant spiral foragers; seed 43 might produce boring straight-line movers; seed 44 might discover a completely novel strategy that neither of the others found. A single run is a single sample from a vast space of evolutionary trajectories. To understand what a configuration *can* produce — and to find the best outcomes — you need multiple independent runs.

This matters beyond just finding higher peak fitness. Two runs might converge to the same fitness score but via completely different behavioral strategies. The observation framework (Option H) can detect this: one run's champions have high velocity alignment (flocking), another's have high movement efficiency (solo foragers), both scoring equally on food gathered. Without multi-seed runs, you'd only ever see whichever strategy your one seed happened to find.

**What we have already:**

The headless runner accepts `--seed N` and the RNG is seeded deterministically. So running the same config with different seeds already produces different evolutionary trajectories. But this is manual — you'd run the command N times with different seed values, manually compare the output CSVs, and hand-pick interesting runs. That's tedious and doesn't scale.

**What's needed:**

1. **Batch launcher.** Run N independent evolution runs in parallel (or sequentially), each with a different seed, same configuration. Collect all results into a structured output directory.

2. **Cross-run comparison.** After all runs complete, summarize: which seed produced the best champion? Which produced the most species diversity? Which showed the most interesting observation metrics? Rank runs by multiple criteria, not just peak fitness.

3. **Integration with the observation system (Option H).** This is where it gets powerful. Each run produces its own observation timeline — fitness curves, velocity alignment, spatial dispersion, species counts. The batch system can compare these *across runs* to surface outliers. "Run 7 had unusually high velocity alignment at generation 40 — worth investigating."

**Batch config:**

The simplest version just wraps the existing headless runner:

```json
{
    "name": "prey_foraging_sweep",
    "base_config": "data/sim_config.json",
    "boid_spec": "data/simple_boid.json",
    "generations": 100,
    "seeds": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    "output_dir": "data/batch_runs/prey_foraging_sweep",
    "save_interval": 10,
    "save_best": true
}
```

Or with auto-generated seeds:

```json
{
    "seeds": { "count": 20, "start": 1 }
}
```

Each run writes to its own subdirectory:

```
data/batch_runs/prey_foraging_sweep/
├── batch_config.json          ← copy of the config used
├── summary.csv                ← cross-run comparison table
├── seed_001/
│   ├── fitness.csv            ← per-generation fitness log
│   ├── observations.csv       ← observer metrics per generation
│   ├── champion_gen100.json   ← final champion
│   └── champion_best.json     ← all-time best
├── seed_002/
│   ├── ...
└── seed_010/
    └── ...
```

**Cross-run summary:**

After all runs complete, the batch system generates `summary.csv`:

```csv
seed, best_fitness, final_mean_fitness, peak_species, final_species, mean_velocity_alignment, mean_movement_efficiency, best_champion_file
1,    487.3,        312.1,              8,            4,             0.12,                     0.34,                      seed_001/champion_best.json
2,    523.8,        298.7,              12,           6,             0.41,                     0.28,                      seed_002/champion_best.json
...
```

This table immediately shows: seed 2 found the highest fitness *and* high velocity alignment — its champions might be flocking foragers. Seed 1 had better movement efficiency — solo navigators. Both are interesting; the batch run found both where a single run would have found only one.

**Parallelism:**

Evolution runs are completely independent — perfect for parallelism. Options:

- **Process-level parallelism (simplest).** The batch launcher spawns N instances of `wildboids_headless` as child processes, each with a different `--seed` and `--output-dir`. On a multi-core machine, this is straightforward and efficient. The launcher doesn't need to understand NEAT internals — it just runs subprocesses and collects results.

- **Thread-level parallelism (more complex but natural in C++).** Run N `Population` + `World` pairs in separate `std::thread`s within a single process. Each thread owns its own `World`, `Population`, `std::mt19937`, and output buffer — no shared mutable state, so no locks needed. C++ makes this straightforward:

  ```cpp
  std::vector<std::thread> threads;
  std::vector<RunResult> results(num_seeds);  // each thread writes its own index

  for (int i = 0; i < num_seeds; i++) {
      threads.emplace_back([&results, i, seed = seeds[i], &config, &spec]() {
          std::mt19937 rng(seed);
          World world(config);
          Population pop(/* ... */, rng);
          for (int gen = 0; gen < generations; gen++)
              run_generation(world, pop, spec, rng);
          results[i] = { seed, pop.best_fitness(), /* ... */ };
      });
  }
  for (auto& t : threads) t.join();
  // results[0..N] now available for cross-run comparison
  ```

  The key thing that makes this safe: `Population` already owns its own `InnovationTracker`, so there's no global state to conflict. Each thread is a fully self-contained evolution run.

  **Data collection for thread-level runs** — two options:
  - **Separate files**: Each thread writes to its own output directory (`seed_001/fitness.csv`, etc.), same format as the process-level approach. File I/O from multiple threads to *different files* is safe. After all threads join, a single-threaded summary pass reads them and produces `summary.csv`. Simple, and the output is identical whether threads or processes produced it.
  - **In-memory collection**: Each thread accumulates per-generation stats into a `std::vector<GenerationStats>` it owns (no disk I/O during the run). After join, the main thread has everything in memory and writes it all at once. Faster for short runs, but uses more memory — trivial for 20 runs × 100 generations, worth considering for 1000+ runs.

  Recommendation: separate files — it's the same format either way, and you can switch between process-level and thread-level freely without changing the analysis pipeline.

  **One gotcha**: if one thread crashes (e.g. a NaN in physics), it takes down all threads. Processes are naturally isolated. Wrapping each thread body in a try/catch and recording failures in `results[i]` mitigates this. With sanitizers and the existing test coverage, crashes in practice are unlikely.

Recommendation: **process-level first** (or a shell script — see below). It's simpler, naturally isolated, and the headless runner is already fast enough (~2 seconds for 100 generations in Release mode). Running 20 seeds sequentially takes ~40 seconds; in parallel across 8 cores, ~5 seconds. Thread-level is a straightforward upgrade when runs get longer (thousands of generations, or much larger populations) and the overhead of process startup or the convenience of in-memory result collection starts to matter.

A simple shell script achieves the minimum viable version immediately:

```bash
#!/bin/bash
for seed in $(seq 1 20); do
    mkdir -p data/batch_runs/sweep/seed_$seed
    ./build-release/wildboids_headless --seed $seed --generations 100 \
        --save-best --output-dir data/batch_runs/sweep/seed_$seed \
        > data/batch_runs/sweep/seed_$seed/fitness.csv &
done
wait
echo "All runs complete"
```

But a proper batch system adds the cross-run summary, observation integration, and structured output that a shell script can't easily provide.

**Parameter sweeps:**

Once batch runs work for multiple seeds, the natural extension is varying *parameters* across runs — not just the RNG seed but configuration values. This is where the real experimental power lives. Evolution is sensitive to ecological parameters in ways that are hard to predict — does doubling metabolism rate produce leaner foragers, or does it just kill everything? Does a larger world select for faster boids or for more efficient navigators? You can hypothesize, but you need to run it to find out.

**What to sweep — three categories of parameters:**

1. **Ecological parameters** — the environment the boids evolve in. These directly shape selection pressure:
   - `metabolism_rate` — how fast boids burn energy by existing. High metabolism = urgency to find food. Low = more time to explore, less pressure to be efficient.
   - `thrust_cost` — energy cost of movement. High = selects for efficient, economical movers. Low = selects for speed and coverage.
   - `food_spawn_rate`, `food_max` — food abundance. Scarce food = intense competition, selects for navigation precision. Abundant = weaker selection, more random drift.
   - `food_eat_radius` — how close you need to be. Small radius = selects for precise steering. Large = coarser approach behavior is fine.
   - Food source mode (`uniform` vs `patch`) and patch parameters — qualitatively different selection landscapes.
   - World size — larger worlds mean food is sparser relative to sensor range, selecting for exploration strategies.

2. **NEAT parameters** — the evolutionary algorithm itself:
   - `weight_mutation_rate`, `weight_perturbation_std` — how aggressively weights change. Too low = slow learning. Too high = can't refine.
   - `add_connection_rate`, `add_node_rate` — structural mutation rates. Controls complexity growth.
   - `compatibility_threshold` — how different genomes must be to form new species. Low = many small species. High = few large species.
   - `population_size` — more individuals = better search but slower per generation.
   - `survival_rate` — selection intensity. Low survival = strong selection, fast convergence but less diversity. High = weaker selection, more exploration.

3. **Body parameters** — the boid's physical capabilities:
   - `max_thrust` values on thrusters — faster boids vs more maneuverable boids.
   - Sensor range, arc width — what the boid can perceive.
   - `linear_drag`, `angular_drag` — how "slippery" vs "sticky" the world feels. High drag = responsive steering. Low drag = momentum-dominated, harder to control.

Each category answers a different question. Ecological sweeps ask "what environment produces the most interesting behavior?" NEAT sweeps ask "what algorithm settings work best for this problem?" Body sweeps ask "what physical capabilities are needed for effective foraging?"

**Sweep config format:**

```json
{
    "name": "metabolism_thrust_sweep",
    "base_config": "data/sim_config.json",
    "boid_spec": "data/simple_boid.json",
    "generations": 100,
    "seeds_per_combo": 5,
    "vary": {
        "metabolism_rate": [0.2, 0.5, 1.0, 2.0],
        "thrust_cost": [0.05, 0.1, 0.2]
    }
}
```

This generates 4 × 3 × 5 = 60 runs: every combination of metabolism rate and thrust cost, each with 5 seeds. The `"vary"` keys are dotted paths into the config — `"metabolism_rate"` maps to `WorldConfig::metabolism_rate`, `"neat.add_node_rate"` would map into the NEAT params, etc.

For sweeping boid body parameters (which live in the boid spec, not the world config):

```json
{
    "vary_spec": {
        "thrusters[0].maxThrust": [30.0, 50.0, 80.0],
        "sensors[0].maxRange": [80.0, 120.0, 200.0]
    }
}
```

**Output structure:**

```
data/batch_runs/metabolism_thrust_sweep/
├── sweep_config.json
├── summary.csv                          ← all combos × all seeds
├── summary_by_combo.csv                 ← averaged over seeds per combo
├── metabolism_0.2__thrust_cost_0.05/
│   ├── seed_1/
│   │   ├── fitness.csv
│   │   ├── observations.csv
│   │   └── champion_best.json
│   ├── seed_2/
│   │   └── ...
│   └── seed_5/
│       └── ...
├── metabolism_0.2__thrust_cost_0.1/
│   └── ...
└── metabolism_2.0__thrust_cost_0.2/
    └── ...
```

The `summary_by_combo.csv` is the key output — it averages across seeds to show the effect of each parameter combination:

```csv
metabolism_rate, thrust_cost, mean_best_fitness, std_best_fitness, mean_species, mean_velocity_alignment, ...
0.2,            0.05,        523.4,             47.2,             6.2,          0.18,                     ...
0.2,            0.1,         487.1,             52.8,             5.8,          0.22,                     ...
0.5,            0.05,        412.6,             39.1,             7.4,          0.31,                     ...
...
```

Multiple seeds per combo are important — they separate "this parameter setting is genuinely better" from "this run happened to get lucky." With 5 seeds, you can compute means and standard deviations; with the observation system (Option H), you can also check whether a parameter setting reliably produces particular *behaviors*, not just particular fitness scores.

**Interaction effects and the value of combinatorial sweeps:**

Parameters interact in non-obvious ways. High metabolism with low thrust cost might produce fast, aggressive foragers. High metabolism with *high* thrust cost might produce boids that barely move — every option is expensive, so they sit and starve. You can't predict these interactions from single-parameter sweeps; you need the full grid (or at least a designed subset like Latin hypercube sampling if the grid is too large).

The observation system makes these interactions visible. A sweep might show: "metabolism 0.5 + thrust_cost 0.1 produces the highest velocity alignment of any combination" — a result you'd never find by sweeping metabolism alone or thrust_cost alone.

**Scaling considerations:**

Full combinatorial grids grow fast. 4 values × 4 parameters = 256 combos × 5 seeds = 1280 runs. At ~2 seconds per run (Release mode, 100 generations), that's ~43 minutes sequential or ~5 minutes on 8 cores. Manageable. But 6 parameters with 4 values each = 4096 combos × 5 seeds = 20,480 runs — now you want thread-level parallelism and possibly a cluster.

For large parameter spaces, alternatives to full grid:
- **Latin hypercube sampling** — sample N points spread evenly across the parameter space. Much fewer runs than a full grid, still covers the space.
- **Sequential refinement** — coarse grid first (2 values per parameter), identify interesting regions, fine grid in those regions.
- **Bayesian optimization** — use results so far to choose the next parameter combination to try. Overkill for this project, but conceptually interesting.

For v1, full grid with sensible parameter counts (2–4 values per parameter, 2–3 parameters varied at once) is plenty.

**Integration with Option I (tournaments):**

Multi-seed runs compose naturally with tournament assembly. "Run this tournament manifest 10 times with different seeds — how consistent is the outcome? Do uniform specialists always beat patch specialists, or does it depend on the seed?" This is statistical power for evolutionary experiments.

**Integration with Option H (observations):**

The observation system is what makes multi-seed runs more than just "pick the highest fitness." Observation metrics computed per-run can be compared across runs to find outliers — runs where unusual behavior emerged. The batch summary should include key observation metrics alongside fitness. Observation triggers (Option H part b) could fire per-run: "save a full population snapshot from any run where velocity alignment exceeds 0.3" — catching interesting emergent behavior across all seeds automatically.

**Implementation sequence:**

1. **Batch config parser** — load the JSON, generate the list of (seed, output_dir) pairs. Trivial.
2. **Batch launcher** — spawn `wildboids_headless` subprocesses with appropriate arguments. Wait for completion. Small — essentially a subprocess manager.
3. **Cross-run summary** — after all runs complete, read each run's fitness CSV (and observation CSV if Option H is implemented), compute summary statistics, write `summary.csv`. Small-medium.
4. **CLI integration** — `wildboids_headless --batch data/batches/sweep.json`. Small.
5. **Parameter sweeps** — extend batch config to vary parameters, generate combinatorial run matrix. Medium.

**Files:**

| File | Action | Purpose |
|------|--------|---------|
| `src/batch_main.cpp` | Create | Batch launcher: parse config, spawn runs, collect results |
| `src/io/batch_config.h/.cpp` | Create | Batch config parser |
| `CMakeLists.txt` | Modify | Add `wildboids_batch` target |
| `data/batches/` | Create | Directory for batch configs |
| `tests/test_batch_config.cpp` | Create | Config parsing tests |

Or, alternatively, fold batch functionality into the existing `wildboids_headless` binary with a `--batch` flag, keeping a single headless executable.

**Effort:** Low-medium. The core (steps 1–3) is simple infrastructure — subprocess management and CSV aggregation. The hard thinking is in *what to compare across runs*, which is really an Option H question. The shell-script version is essentially free and useful today; the structured version adds reproducibility and observation integration.

---

### Option K: Directional mouth — eating requires orientation and approach (low effort, interesting evolutionary pressure)

**The problem it solves:** Currently, both food eating (`check_food_eating`) and predation (`check_predation`) are pure distance checks from the boid's center. A boid can eat food by drifting backwards over it, or a predator can "catch" prey that bumps into its tail. There's no selection pressure for boids to *aim* at food or prey — any contact works. This weakens the evolutionary signal for good sensor-thruster coordination, since random flailing that happens to pass over food is rewarded equally to precise oriented approach.

**What it adds:** A directional "mouth" mechanic. To eat (food or prey), two conditions must hold beyond the existing distance check:

1. **Arc check (food is in front).** The target must fall within a forward-facing arc (the "mouth"). Uses the same `angle_in_arc()` logic already used by the sensor system — compute the delta to the target in body frame, check it falls within a configurable arc centered on the boid's forward direction. This is the same math as sensor arc detection, just applied to eating.

2. **Velocity dot check (boid is moving toward it).** `dot(boid.velocity, delta_to_target) > 0`. This prevents a boid that's reversing (velocity opposite to heading) from eating things that happen to be "in front" geometrically. The boid must actually be *approaching* the target, not backing over it.

Both checks are cheap — one body-frame rotation + arc test (already have the utility function), one dot product. Two extra lines per eating check.

**Implementation sketch:**

In `check_food_eating()` and `check_predation()`, after the existing distance check passes:

```cpp
// Arc check: target must be in front of the boid's "mouth"
Vec2 body_delta = delta.rotated(-boid.body.angle);
float angle = std::atan2(body_delta.x, body_delta.y);  // +Y = forward
if (!angle_in_arc(angle, 0.0f, config_.mouth_arc_width))
    continue;  // not in front

// Velocity check: boid must be moving toward the target
if (delta.dot(boid.body.velocity) <= 0.0f)
    continue;  // moving away or stationary
```

**Configuration (in `sim_config.json`):**

```json
{
    "mouth": {
        "enabled": true,
        "arcWidth": 90,
        "requireApproach": true
    }
}
```

- `enabled` — toggle the whole mechanic (off by default for backwards compatibility with existing champions).
- `arcWidth` — mouth arc in degrees. 180° = front hemisphere (generous). 90° = must be roughly ahead (moderate). 45° = precision aiming required (hard mode). Start generous, tighten over evolutionary time if desired.
- `requireApproach` — whether the velocity dot check is active. Could disable this independently to test arc-only vs arc+approach.

Both prey eating food and predators catching prey would use the same mouth parameters (or could be split into per-type configs if predators and prey should have different mouth widths).

**Evolutionary pressure this creates:**

- **Stronger sensor-thruster correlation.** Boids must orient toward food/prey before eating, so evolution must discover "food detected in left arc → steer left" mappings. Random-weight brains that stumble over food backwards get nothing.
- **Rewards precise steering.** A tighter mouth arc selects for boids that can aim accurately, not just move in the right general direction.
- **Penalises reversal strategies.** The velocity check means boids can't evolve a "reverse into food" exploit. They must face and approach.
- **Natural difficulty scaling.** Start with a wide mouth (180°) for early evolution when brains are random, tighten it as populations improve. Could even make mouth width a curriculum parameter that narrows over generations.

**Risk: weaker early fitness gradient.** Random-weight boids already struggle to find food. Adding a mouth requirement means even accidental food contact often doesn't count (wrong orientation or backing over it). This could flatten the early fitness landscape, making it harder for evolution to get traction. Mitigations:
- Start with a generous arc (180°) — most forward-moving food encounters still count.
- The velocity check has a natural escape hatch: boids that move forward a lot (already selected for by the existing fitness gradient) will pass the velocity check for anything in their path.
- Could phase in the mouth: first N generations use the current omni-directional eating, then switch on the mouth. Or start with 360° and narrow over time.

**Testing:**

- Boid moving forward over food within mouth arc → eats (existing behavior preserved for forward approach).
- Boid moving forward over food outside mouth arc (food to the side/rear) → doesn't eat.
- Boid reversing over food that's geometrically "in front" → doesn't eat (velocity check).
- Stationary boid on top of food → doesn't eat (velocity check, unless we add a small threshold).
- Predator catching prey: same arc + approach checks.
- Mouth disabled → reverts to current omni-directional behavior.

**Effort:** Low. The core is ~10 lines of new logic in two existing functions, plus config parsing. The `angle_in_arc()` utility and body-frame rotation pattern are already established. Main work is testing edge cases and tuning the arc width for good evolutionary dynamics.

**Interactions with other options:** Composes well with Option H (observations) — mouth hit rate (successful eats / food within radius) would be a useful observation metric showing how well boids have learned to aim. Also interacts with Option E (food source experiments) — patchy food with a mouth requirement would strongly select for boids that can navigate *to* a patch and then methodically sweep through it with good orientation, rather than just blundering through.

