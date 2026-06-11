# Wild Boids — Feature History & Design Choices

A retrospective companion to [build_log.md](build_log.md) (the step-by-step record) and
[forward_plan.md](forward_plan.md) (the plan). This doc reads the whole paper trail and pulls out
**(1)** the main features that have been iteratively layered onto the program and the boids, and
**(2)** the coding, philosophical, and bio-mimicry choices made along the way.

Wild Boids began as a Java predator/prey boid simulation (2007) and is being rebuilt in C++.
The 2007 version used Reynolds-style *kinematic* steering (velocity set directly by rules). The
rebuild throws that out in favour of **thrust-based physics + evolved neural control**, for
reasons that turn out to drive almost every design decision below.

---

## Part 1 — The feature stack, in the order it was built

Each layer was added on top of a working, tested previous layer. Tests accompany every step
(28 → 266+ over the course of the build).

### Foundation: physics and bodies (Phase 0)

- **2D rigid body** — position, velocity, angle, angular velocity, mass, moment of inertia.
  Integrated with **semi-implicit Euler** (update velocity, then position).
- **Thrusters** — body-local force generators. Each has a position relative to the centre of
  mass, a firing direction, a max thrust, and a power level ∈ [0,1]. Force *and* torque emerge
  from where the thruster sits (the lever arm), not from a special-case turning rule.
- **The 4-thruster boid** — rear (forward propulsion), front (braking), left-rear and right-rear
  (steering torque). The spec format allows any number/layout; 4 was the default.
- **Drag** — linear and angular drag as the only thing stopping infinite acceleration / endless
  spin. Drag is the **proxy for fluid resistance** (water/air); it gives boids a terminal velocity
  and natural coasting.
- **JSON boid specs** — boids are data, not code. Thrusters, mass, inertia all load from JSON.

### Seeing it move (Phase 1)

- **SDL3 renderer** — boids as oriented triangles, thruster indicator lines, dark background,
  prey green / predator red.
- **Fixed-timestep main loop** (120 Hz), pause/resume, toggleable overlays.
- **Random wander** — an explicitly *throwaway* pre-brain behaviour, just to make the scene lively
  before brains existed.

### A world with structure (Phase 2)

- **Toroidal (wrap-around) world** — single source of truth for wrap math (`toroidal_delta`).
- **Spatial grid** — uniform grid for O(nearby) neighbour queries instead of O(n²), validated
  against brute force. Everything that needs "who's near me" rides on this.

### Perception (Phase 3)

- **Sensory system** — wedge-shaped **sensor arcs** that detect nearby entities and emit
  normalised floats (`1.0 = touching, 0.0 = nothing`). Default boid got 7 arcs (5 forward, 2 rear).
  This is the boid's input vector.

### Brains (Phase 4) — *evolved, not trained*

- **NEAT neural networks** — the "brain" maps the sensor float-array to the thruster float-array.
  Feed-forward, built from a genome via topological sort, with per-node activation functions
  (Sigmoid on outputs → naturally [0,1] → thruster power).
- **One-tick delay** — brain outputs take effect on the *next* physics tick, mimicking real neural
  propagation delay.
- **Important:** these networks are **evolved by neuroevolution (NEAT), never trained by
  back-propagation.** There is no gradient descent anywhere in the project. Weights *and* topology
  are discovered by mutation + crossover + selection. (The planning docs deliberately cite research
  contrasting neuroevolution with backprop-trained nets.) If you've been thinking of these as
  "back-propagation neurons," they're the opposite school: Darwin, not gradient descent.

### Evolution (Phase 5)

- **Mutation operators** — perturb/replace weights, add connection, add node (complexify),
  toggle and delete connections.
- **Innovation tracking** — historical markings so the same structural mutation gets the same ID,
  which is what makes meaningful crossover possible.
- **Crossover** — innovation-aligned recombination of two parents.
- **Speciation** — genomes grouped by compatibility distance; fitness sharing within species;
  stagnant species culled; elitism preserves champions. This protects new structural ideas long
  enough to be tuned.
- **XOR benchmark** — a deliberate sanity test: NEAT must *grow* a hidden node to solve XOR,
  proving mutation + crossover + speciation + selection actually work together before trusting them
  on boids.
- **Food, energy, metabolism, death** — food spawns; eating adds energy; metabolism and thrust
  both cost energy; energy ≤ 0 kills the boid. **Energy becomes the universal currency.**
- **Fitness** = energy gained → the gradient evolution climbs.
- **Headless runner** — no-graphics CLI for fast evolution runs, CSV output, champion saving.
  Release build runs ~30× faster than the sanitiser-instrumented debug build.

### Sharper foraging and configurable worlds (Phase 5.5b–5.7)

- **Food sensors** — sensor arcs that detect *food* specifically, giving evolution a direct
  "food is over there" signal instead of relying on blind area-sweeping.
- **Shared `sim_config.json`** — GUI and headless load identical physics so a champion replays
  exactly as it evolved.
- **Food-source strategies** (`std::variant`): **uniform** scatter vs **patches** (clustered
  sites), the latter creating real foraging pressure.

### Predators (Phase 5b)

- **Predator boids + predation** — predators catch prey within a catch radius and gain energy;
  they don't eat food.
- **Dual-population co-evolution** — two independent NEAT populations (prey + predator) evolving
  in the *same* world simultaneously. Prey selected for foraging (and implicitly evasion),
  predators for catching.
- **Champion packaging** — a Python helper bundles a matched prey + predator + the exact config
  into a dated folder so a "matchup" stays reproducible even as later runs overwrite loose files.

### Refining the body and senses (Options K–N)

- **Directional "mouth" (Option K)** — eating now requires the boid to **face the target and be
  moving toward it** (forward arc check + velocity dot-product), instead of absorbing food from any
  direction. The mouth is, in effect, *at the front*. Off by default; gated by config.
- **Compound eyes (Option L)** — replaced the per-filter sensor arrays with **16 multi-channel
  "eyes,"** each reporting food / same-type / opposite-type simultaneously (≈49 inputs). One eye
  can say "food *and* a predator at 2 o'clock." Prey and predator specs become identical except for
  the `type` field; "same vs opposite" is resolved at runtime.
- **Proprioception (Options C2/C3)** — **speed sensor**, **angular-velocity sensor** (knows its own
  spin → can damp oscillation), and a **noise sensor** (an internal random source evolution can
  choose to exploit for jitter/exploration, or ignore). These are *interoceptive* — the boid
  sensing its own body, not the world.
- **Two-tier vision (Option M)** — a second set of **long-range, narrow eyes** alongside the
  short-range wide ones. Coarse distant detection vs fine near detail; rendered in a different
  colour. Designed so the two tiers can later evolve differently.
- **Per-boid metabolism (Option N)** — predators can burn energy faster than prey, so they must
  hunt to live (fixing "immortal predators" in early co-evolution runs).

### Memory, efficiency, flocking, and evolvable bodies (post-build-log, from git history, Mar 2026)

These landed after the build-log narrative and are visible in the commit history:

- **Recurrent connections** — optional backward/self connections that read the *previous* tick's
  value. This gives the network **memory**: a self-connection becomes a leaky integrator; an
  output→hidden loop lets a boid sense "what was I doing last tick?" Off by default; feed-forward
  champions keep working unchanged.
- **Net-energy fitness** — selectable fitness mode: **net** (energy gained − energy spent) instead
  of **gross** (energy gained). This adds direct selection pressure for *efficiency*, not just
  intake — wasteful thrusters now hurt.
- **Shoaling drag reduction — the "aerodynamics" proxy.** Same-type neighbours within a forward
  arc **reduce a boid's effective drag** (up to a configurable max). This is the in-sim stand-in
  for the real **hydrodynamic/aerodynamic benefit of flocking/schooling** (fish in schools use
  ~50%+ less energy via drafting/slipstreaming). Crucially it makes collective movement pay off
  *intrinsically through energy*, rather than by bolting "reward flocking" onto the fitness
  function. A matching **shoaling sensor** lets the boid perceive its drafting benefit. This is the
  feature behind the "first collective movement" and "predator flocking" milestones.
- **Evolvable sensory morphology** — a **morphology genome** carried alongside the NEAT brain genome:
  per-eye **angles and arc widths** mutate and cross over, so evolution shapes *where the boid looks
  and how finely*, not just how it thinks. ("1st evolving senses generation," 4 Mar 2026.)
- **GUI/observability polish** — death-indication lines, colour-blind-friendlier blue/orange
  palette, longer-but-narrower sensor visualisation, sensor-debug CSV dump, and a Python video-reel
  tool (with info cards and audio) for turning runs into shareable clips.

---

## Part 2 — Bio-mimicry choices

The recurring principle: **make the simulated constraint mirror a real biological one, then let
evolution discover the behaviour** — rather than scripting the behaviour directly.

- **Locomotion as thrust, not teleportation.** Movement maps to how animals actually move in a
  horizontal plane, which biology reduces to ~3 control axes: forward thrust, yaw (turning), and
  braking. The 4-thruster layout maps to **caudal fin / wing downstroke** (rear propulsion),
  **spread pectoral fins / flared wings** (front braking, deliberately weaker and costly), and
  **differential fin/wing drag** (the rear side thrusters for turning).
- **The speed-vs-manoeuvrability trade-off** is built into the physics, echoing the fish **BCF
  (cruising) vs MPF (manoeuvring)** distinction — you can't max propulsion and max turning at once.
- **Steering thrusters at the rear** exploit the **moment arm**: force far from the centre of mass
  produces rotation. Same reason a fish's tail and a boat's rudder sit at the back.
- **No strafing.** Boids (like most animals) must rotate before they can change direction, giving
  realistic turning arcs — unlike a holonomic drone. (The docs explored a holonomic 6-thruster
  layout as an *option*, precisely because it's the un-animal-like alternative.)
- **The mouth is at the front.** Eating requires facing and approaching the target — predators must
  actually aim and commit, not vacuum up prey from behind.
- **Compound eyes.** Multi-channel arcs echo insect/animal vision: many directional receptors,
  finer resolution forward, and a near/far two-tier system (high-res close-up, coarse long-range
  scanning). Evolving eye angle/width mirrors the **co-evolution of sensors and brains** in real
  organisms.
- **Interoception.** Sensing own speed and angular velocity mirrors the **vestibular/proprioceptive**
  sense — animals know how fast they're going and spinning.
- **Energy as the master constraint** — every action costs energy, eating restores it, starvation
  kills. This follows the **Tu & Terzopoulos "Artificial Fishes"** insight that *energy-efficient
  movement looks natural*, so optimising for energy naturally yields lifelike motion.
- **Schooling pays for itself.** Shoaling drag reduction reproduces the genuine **energetic benefit
  of collective movement**, so flocking can emerge as an *adaptive* strategy (cheaper travel, plus
  safety in numbers under predation) rather than a hand-coded rule.
- **Co-evolution and asymmetry.** Predator and prey evolve against each other (a Red-Queen dynamic),
  and can differ physically (e.g. metabolism), mirroring real predator/prey arms races.

Touchstones cited throughout: **Reynolds' boids** (what we're moving beyond), **Braitenberg
vehicles** (simple sensor→motor wiring → rich behaviour), **Karl Sims' evolved virtual creatures**
(co-evolving body + brain under physics), and the **ALIEN** artificial-life simulator.

---

## Part 3 — Coding choices

- **Three loosely-coupled layers, float arrays between them:** Sensors → Processing Network →
  Thrusters. No layer knows the internals of another; each is independently testable and swappable.
  Sensor count, network topology, and thruster layout can all change without touching the others.
- **Simulation is a pure library with zero graphics dependencies.** The renderer is a *consumer* of
  state, never a producer. This is what makes the headless evolution runner possible at all.
- **Composition over inheritance, value types where possible.** A `Boid` *owns* a rigid body,
  sensors, and a brain.
- **`Boid` is move-only.** Holding the brain via `unique_ptr<ProcessingNetwork>` made the boid
  non-copyable — a deliberate accepted consequence (container code uses `emplace`/`std::move`).
- **Polymorphic brain interface.** `DirectWireNetwork` (a fixed-weight test fixture) and
  `NeatNetwork` are interchangeable behind `ProcessingNetwork`, so the pipeline could be tested
  before NEAT existed.
- **Strategy pattern via `std::variant`** for food sources (uniform vs patches) — no inheritance,
  dispatched with `std::visit`.
- **Backward compatibility treated as a feature.** Every format change keeps old champions loading:
  legacy 7/11-sensor specs still run alongside compound eyes; recurrence/morphology/per-boid
  metabolism are all opt-in with sentinels or `std::optional`. Old genomes replay identically.
- **Two-level channel system** — the *boid spec* defines network **structure** (which input slots
  exist, baked into the genome), while *sim_config* is a **runtime gate** (a disabled channel
  outputs 0 without changing the genome). Lets you run "food-only" experiments without retraining.
- **Derive, don't hardcode.** NEAT input count comes from `sensor_input_count(spec)`, never a magic
  number — so adding a sensor type doesn't require touching the brain code.
- **Innovation numbers preserved through JSON** — essential for crossover alignment, so
  serialisation can't quietly break evolution.
- **Shared config for identical replay.** GUI and headless read the same `sim_config.json`; the
  champion-packaging tool snapshots the config so a saved matchup stays physically reproducible.
- **JSON is `camelCase`, C++ is `snake_case`,** with translation isolated in the IO layer.
- **Test-first, always.** Catch2, one file per component; tests run after every step. The
  **XOR benchmark** validates the whole evolutionary machine. **ASan/UBSan in debug builds** caught
  real bugs immediately — e.g. a heap-use-after-free where `mutate_add_node` held a vector reference
  across a `push_back` reallocation.
- **Reproducibility by seed** — runs are deterministic given a seed, so results can be compared and
  re-examined.

---

## Part 4 — Philosophical choices

- **Physics-grounded to prevent "hacking."** This is the founding decision. The 2007 kinematic model
  let evolved boids make physically impossible moves (instant reversal, free turning) and face no
  efficiency pressure. Real forces, inertia, and energy cost **close those loopholes** — evolution
  must respect constraints, and those constraints become part of what it shapes.
- **Neuroevolution, not back-propagation.** There is no supervised target to descend a gradient
  toward — only survival in a world. NEAT discovers both the **weights and the topology**,
  *complexifying from a minimal network*: start as simple as possible, add nodes/connections only
  when they earn their keep. This is deliberately Darwinian rather than engineered.
- **Emergent, not programmed.** Reynolds-style boids *encode* separation/alignment/cohesion as
  rules. Here, any flocking, pursuit, or evasion must be **discovered** by the sensor→brain→thruster
  pipeline. The architecture is the hypothesis; the behaviour is the result.
- **Intrinsic pressures over imposed goals.** The strong preference is to let behaviour arise from
  the boid's *ecological situation* (food, predators, energy, drafting) rather than by adding terms
  like "reward flocking" to fitness. The docs name this tension explicitly: **simulation-as-
  experiment** ("what emerges from these pressures?") vs **simulation-as-tool** ("can we evolve a
  target behaviour, to test the architecture's expressiveness?"). Steerable fitness is kept as an
  option, not the default — flocking via shoaling energy is the in-spirit version.
- **Selecting for efficiency, not just intake.** The move to *net* energy fitness reflects the view
  that a real organism's success is energy *budget*, not gross consumption.
- **Reproducibility and provenance matter.** Seeded runs, shared configs, and dated champion
  packages exist so an interesting result can be returned to and trusted later — treating runs as
  experiments with a record, not one-off demos.
- **Observability is an open problem.** A noted limitation: the interesting things (flocking,
  pursuit, correlated movement) are *visible* in the GUI but not yet *measured*, so evolution can't
  see them either. Building an observer layer (velocity alignment, group structure, sensor→thruster
  correlation) is a recognised next direction — and would let two equally-fit-but-behaviourally-
  different strategies be told apart.
- **Honest about simplifications.** The docs repeatedly flag where the model bends reality
  (drag as a stand-in for fluid dynamics; the one-tick delay; 2D projection of 3D locomotion) and
  treat each as a tunable design decision rather than a hidden assumption.

---

*Sources: this synthesises [build_log.md](build_log.md), [forward_plan.md](forward_plan.md),
[boid_theory.md](boid_theory.md), [boid_theory2.md](boid_theory2.md),
[evolution_theory.md](evolution_theory.md),
[evolution_neuralnet_thrusters.md](evolution_neuralnet_thrusters.md),
[sense_system_planning.md](sense_system_planning.md), [spec.md](spec.md), [dan_log.md](dan_log.md),
and the git commit history through June 2026.*
