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

## What comes next (orientation)

- **Phase 1**: Simple rendering (SDL3 window, draw boids as triangles, watch them move) — **detailed plan below**
- **Phase 2**: Spatial grid + toroidal distance functions
- **Phase 3**: Sensory system (sensor arcs, read from spatial grid)
- **Phase 4**: Processing network (NEAT or simpler, connecting sensors to thrusters)
- **Phase 5**: Evolution loop (fitness, selection, crossover, mutation)
- **Phase 6**: Analytics, data collection, ImGui controls, ImPlot graphs

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
