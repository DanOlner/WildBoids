# Wild Boids 2.0: Planning Summary

Prompt: "In @toplevel_planner.md please add a section summarising where we've got to so far in the existing planning docs - @plan_newplatform.md, @evolution_theory.md and @boid_theory.md. We'll then use this as a basis for mulling a starting point."

**Purpose:** Consolidate the state of play across our three planning documents and identify the key decisions needed before we start building.

---

## Where We've Got To

### 1. Platform ([plan_newplatform.md](plan_newplatform.md))

**Verdict: JavaScript in the browser, custom-built.**

- JavaScript in 2026 is more than capable. The original 150-boid Java simulation is trivial for modern JS; benchmarks show 1,000+ boids at 60fps with basic Canvas 2D, 4,000+ with WebGL.
- **Rendering:** Start with Canvas 2D for prototyping, upgrade to PixiJS v8 (WebGL/WebGPU) for production.
- **Threading:** Simulation in a Web Worker, rendering on the main thread, connected via SharedArrayBuffer.
- **Spatial partitioning:** Uniform grid (best for uniform-sized agents).
- **Framework:** Custom, not a general ABM library. Wild Boids has specific needs (evolved sensory systems, toroidal world, genetic crossover) that don't fit off-the-shelf frameworks.
- **Phased approach:** (1) Canvas 2D prototype → (2) core sim + evolution → (3) Web Worker + PixiJS → (4) WebGPU / extras.

### 2. Evolution System ([evolution_theory.md](evolution_theory.md))

**Three interconnected subsystems designed together:**

**Sensory system** — how boids perceive the world:
- Original used 6 polygon FOVs (3 for own kind, 3 for other kind). Expressive but expensive.
- Recommendation: **parametric sectors** (center angle, width, radius) — cheaper, still evolvable.
- 3-5 sensors per boid. Include velocity info in detection (original only used position).
- Each sensor has evolved response weights mapping detections to actions.

**Genetic representation** — how behavior is encoded:
- **Direct parameter encoding** (like the original), not neural networks. Interpretable, simple, sufficient.
- Normalize all gene values to 0-1 for clean crossover/mutation.
- Group related genes (all parameters for one sensor together).
- Predators and prey may need asymmetric genome structures.

**Evolution mechanism** — how populations improve:
- **Survival-time fitness** (implicit: older boids have survived longer, so they're fitter).
- **Tournament selection** (more controllable than the original's cube-root bias).
- **Uniform crossover** with **Gaussian mutation** (~5% rate, small magnitude) — the original lacked mutation entirely.
- Coevolution risks: Red Queen cycling, population collapse. Mitigations: spatial breeding, possibly hall-of-fame testing.

### 3. Physics & Locomotion ([boid_theory.md](boid_theory.md))

**Major departure from the original: replace pseudo-physics with thrust-based rigid body dynamics.**

**The problem:** Reynolds-style kinematic boids have no real physics. The 2007 Wild Boids evolved to "hack" the pseudo-physics — making physically impossible turns, exploiting the gap between the model and reality.

**The solution:** Each boid is a 2D rigid body with thrusters. The boid brain doesn't set velocity directly — it controls thruster power levels, and movement emerges from real forces, torques, mass, inertia, and drag.

**Two proposed models:**

- **Model 1 (Fixed layout):** Hardcoded thruster positions (e.g. front brake, rear main, two rear-side for rotation). Simple, interpretable, no body-plan evolution.
- **Model 2 (Genome-defined layout):** Thruster positions and angles are genes. Set at birth, fixed for life. Same physics engine, zero extra runtime cost. Subsumes Model 1 as a special case. Supports a **mutability flag** per gene: locked (= Model 1 behavior), mutable (body-plan evolution), or partially mutable (some locked, some free).

**Recommendation:** Implement Model 2 from the start (barely harder than Model 1), but begin with locked thruster geometry and a sensible seed layout. Unlock mutability later once brain evolution is working.

**Energy system:** Thrusters cost energy. Baseline metabolism drains energy. Eating replenishes it. This naturally selects for efficient locomotion and prevents physics-hacking — wasteful strategies starve.

**Biological grounding:** The thruster model maps well to real animal locomotion in 2D: forward thrust (caudal fin / wings), yaw torque via differential side forces (pectoral fins), braking via increased drag. Fish BCF/MPF trade-offs confirm the speed-vs-manoeuvrability tension is biologically real. Braitenberg vehicle literature confirms simple sensor→actuator mappings produce rich emergent behavior.

---

## Key Decisions Still Needed

These are the choices we need to make (or at least pick starting values for) before coding:

### A. Thruster Configuration to Start With

| Option | Thrusters | Brain outputs | Genome genes (body) | Notes |
|--------|-----------|--------------|---------------------|-------|
| 2-thruster differential | 2 | 2 | 6 (if mutable) | Most elegant, smallest genome. Forward = both fire. Turning = differential. No reverse. |
| 3-thruster (no brake) | 3 | 3 | 9 (if mutable) | Biologically honest — deceleration via drag only. |
| 4-thruster (with brake) | 4 | 4 | 12 (if mutable) | Most expressive. Allows active braking strategies. |

Start locked or mutable?

### B. Sensor Count and Structure

- How many sensors per boid? (3? 5? Different for predators and prey?)
- Separate sensors for own-kind vs other-kind, or generic sensors that detect both?
- What info per detection? (count in sector? nearest distance? average angle?)

### C. Genome Architecture

- Total genome size depends on A + B: sensors × (sensor params + thruster weights) + thruster position genes + global params.
- Should predator and prey genomes be identical in structure?

### D. World Setup

- Toroidal (wrap-around) like the original, or bounded?
- World size relative to boid perception range?
- Prey food: static resource patches, or something else?
- Starting population counts?

### E. Evolution Triggers and Rates

- When does breeding happen? (On death, like original? Periodic? Continuous?)
- Population management: fixed count (replace dead immediately) or variable?
- Mutation rate and magnitude: start at what values?

### F. What to Build First

The plan_newplatform.md phasing assumed the original FOV system and Reynolds-style physics. With the thrust model, the phases might look different. A possible revised order:

1. **Physics sandbox:** Get a single boid with thrusters moving correctly on Canvas 2D. Tune drag, thrust, mass, inertia until it "feels" right. No sensors, no evolution — just manual or random thruster inputs.
2. **Sensors + brain:** Add the sensory system and sensor→thruster weight mapping. Hand-design a few boids to verify sensors work (e.g. a boid that flees a moving point).
3. **Population + evolution:** Multiple boids, predator/prey dynamics, genetic crossover + mutation. This is where it gets interesting.
4. **Performance:** Web Worker, PixiJS, spatial grid. Scale up.
5. **Body-plan evolution:** Unlock thruster position mutability.

---

## Document Map

| Document | Contents | Status |
|----------|----------|--------|
| [oldjavacode_summary.md](oldjavacode_summary.md) | Analysis of original 2007 Java codebase | Complete |
| [plan_newplatform.md](plan_newplatform.md) | JavaScript platform feasibility, stack recommendations | Complete |
| [evolution_theory.md](evolution_theory.md) | Sensory systems, genetic representation, evolution mechanisms | Complete (starter) |
| [boid_theory.md](boid_theory.md) | Thrust-based physics model, energy system, genome-defined thrusters | Complete (starter) |
| **toplevel_planner.md** | **This document** — consolidated summary and decision points | Current |
