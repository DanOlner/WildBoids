## Wild boids spec doc #1

### Dan initial spec [16.2.26]

A relatively quick braindump of all the elements to fit together, based on various conversations in other md files so far. See the list of those convos below.

From all that, let's just lay out a top level list of the elements.

Principles:

- Keep everything as loosely coupled as possible. No single element should need to know about any other.
- Think through this sequentially, doing the minimal structural elements first that we need for it to be testable, before incrementally adding extra elements.

Elements:

- The main sim code - from @plan_newplatform.md: "The simulation engine should be a pure library with no graphics dependencies. It takes inputs (time step, world parameters), updates internal state, and exposes read-only state for anyone who wants to look at it. The renderer is a consumer of state, never a producer."
- This will need to instantiate any initial runs, making the boids, positioning, randomising etc.
- Front end / GUI. This isn't even a necessary component - the simulation should be able to run statelessly without linking to this front end. But I'm mentioning first, as I want to think through this:
    - How to load in / save 'flocks' i.e. the exact spec for a list of boids. If we get a basic version of this done to start with, and can load in via the front end GUI (while also providing command line options to load and run) that will help set things up. So we'll need that spec. Which will itself maybe require more than one script, if we're really keeping things separate like e.g. the boids' thruster configuration separate from its sensory configuration, separate from the senses-to-thrust-output 'brain' or stimulus/response system.
    - We will need that even if doing the simplest 'just testing the physics' phase. In fact, if we keep the 'brain' separate, we can make the simplest version e.g. 'just fire randomly' that doesn't even need sensory input. Or 'simulate' sensory input.
    - The GUI will also need things like: stop / start / pause / save current entire state (including boids' vectors so can kick off from same point) / open analytics dialogue (for export mainly) ... leave open so more can be added.
- The per-tick simulation loop, in order (claude edit of my initial list):
    1. **Physics engine**: apply forces from last tick's thruster commands, integrate positions/velocities, handle world boundaries (toroidal wrapping). Simple 2D rigid body physics.
    2. **Collision / interaction resolution**: did a predator actually catch a prey this tick? Resolve contact events. Uses the same spatial grid as sensors but has different consequences (energy transfer, prey death).
    3. **Energy / metabolism**: debit energy for thruster use, credit survival time (prey) or catches (predators), kill boids that hit zero energy. Could be part of physics but worth keeping separate — it's the selection pressure that drives evolution.
    4. **Sensory engine**: query the spatial grid for each boid's sensor arcs. Requires read access to a shared view of the world. May require spatial partitioning for speed (uniform grid, as discussed in @plan_newplatform.md).
    5. **Brain engine**: passes sensor signals into the processing network (NEAT or similar), which outputs thruster commands. Those commands are *stored*, not applied yet — they become the forces applied in step 1 of the *next* tick. This one-tick delay is physically correct and avoids order-of-update bugs.
- The per-generation outer loop (wrapping the per-tick loop):
    - **Evolution engine**: when a generation ends (after N ticks, or when population drops below threshold, or other criteria), evaluate fitness, run NEAT speciation, crossover, mutation, spawn new population. This is the outer loop; the per-tick steps above are the inner loop.
    - Generation boundaries also trigger population snapshots, metric logging, etc.
- Boids: basic setup is -->
    - They have a 'front' and 'back' but their thrusters can sit anywhere on a circle surrounding their centre of gravity. We may constrain thruster direction to be: "can't thrust towards the boid's body' (despite them not really having a body). Front and back is only a way for us programmatically to orient sensor organisation e.g. front-sensors will be a certain position.
    - They have a centre-point or small circle detector that is a 'mouth', both for detecting 'eating' (prey eating grass, predators eating prey). That may need testing if boids fail to eat well...
    - Their sensor array positions and thrusters will have positions relative to their centre point, which is also their rigid body centre (I think).

- The analytics code bundle should have this ability: look our for certain conditions (like good predator/prey balance, a certain level of complexity, a certain correlation of alignment at sample points) and be able to trigger other actions e.g. save the boid world's whole state at that point and label it.





### Spatial grid issues / doughnut world

On the toroidal world: boids can't just jump across the doughnut. Detection needs to flow smoothly across the edges as though there wasn't and edge. So e.g. a boid detection x pixels to the 'right' will need to be able to pick up things in its sense range wrapped round to the left:

Claude response: Yes, this is a real issue and it affects the spatial grid design specifically. The uniform grid we planned (see @plan_newplatform.md §Spatial Partitioning) needs to handle wrapping at two levels:

**1. Distance calculation.** Every distance/direction calculation between two boids must use the *toroidal minimum distance* — the shortest path considering wrapping. For a world of width W and height H:

```
dx = boid_b.x - boid_a.x
if (dx >  W/2) dx -= W;
if (dx < -W/2) dx += W;
// same for dy
```

This is cheap — just a couple of comparisons per pair. It needs to be baked into the distance function used everywhere: sensors, collision detection, nearest-neighbour queries. If we centralise this in one `toroidalDelta(a, b)` function, nothing else needs to think about wrapping.

**2. Spatial grid queries near edges.** This is the trickier part. A boid sitting at x=5 with a sensor range of 100 needs to query grid cells at the *right edge* of the world (x=W-95 to x=W). The grid itself doesn't wrap — it's a flat 2D array of cells. So queries near edges must check wrapped cell indices.

Two approaches:

- **Ghost cells (simplest):** Pad the grid with one extra ring of cells on each side that mirror the opposite edge. When you insert a boid at (5, 10), also insert a reference to it in the ghost cells at (W+5, 10). Queries never need to know about wrapping — they just see boids in adjacent cells as normal. Cost: slightly more memory, slightly more insertions per tick. This is the standard approach in particle simulations.

- **Wrapped index lookup:** No ghost cells. Instead, the grid query function wraps cell indices with modulo arithmetic: `cell_x = ((query_x % cols) + cols) % cols`. Cheaper on memory, but every grid query must use the wrapping index function. Slightly more branching per query.

Ghost cells are probably better for us — the memory cost is negligible (one extra ring around the grid), and it means the sensor arc intersection code doesn't need to care about wrapping at all. It just queries nearby cells and gets correct results.

**3. Sensor arcs crossing the edge.** A sensor arc centred at x=5 pointing left (towards x=0 and wrapping to x=W) needs to detect boids on both sides of the wrap boundary. With ghost cells, this works automatically — the ghost cells at negative x-indices contain references to boids near x=W. With wrapped index lookup, the arc-cell intersection code needs to handle the case where the arc spans the boundary. Ghost cells keep this simple.

**What this means for the plan:** The spatial grid module needs a `toroidal` flag. When true, it allocates ghost cells and double-inserts boids near edges. The rest of the system — sensors, collisions, physics — calls the same grid query API and gets correct toroidal results without knowing the wrapping exists. This fits the loose coupling principle: wrapping is the grid's problem, not the sensor's problem.


### Existing planning documents [claude-gathered]

- **oldjavacode_summary.md** — Analysis of the original 2007 Java codebase: what each script does, how they connect, and what the whole project did.
- **evolution_theory.md** — Exploration of approaches for evolving boid sensory systems and behaviors, covering sensory design, genetic representation, and evolution mechanisms.
- **boid_theory.md** — Design for replacing Reynolds-style pseudo-physics with thrust-based physical locomotion, where boids have actual thrusters producing forces and torques on a rigid body.
- **plan_newplatform.md** — Platform assessment for reimplementing the simulation, evaluating JavaScript feasibility, frameworks, and performance considerations.
- **toplevel_planner.md** — Consolidation of the state of play across the other planning documents, identifying key decisions needed before building.
- **evolution_neuralnet_thrusters.md** — Modular, evolvable framework connecting sensory inputs to thruster commands via a NEAT-like neural processing layer where structure, weights, and learning rules can evolve.
- **fromjava_gotchas.md** — Practical guide for moving from Java to C++: mental model differences, build/test workflow, and project-specific pitfalls to watch for.

