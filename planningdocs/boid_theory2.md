# Boid Movement Physics: Maximising Evolvable Movement Space

Claude-drafted.

## Core Question

How do we give evolving boids the richest possible movement phenotype space using thrust-based physics on a 2D plane?

## Key Physics Considerations

### Force-Torque Space

Any thruster configuration defines an **attainable force-torque space** — the set of (Fx, Fy, torque) combinations achievable simultaneously. A good layout fills a 3D volume; a bad one only reaches a thin slice. This is the central metric for movement richness.

- **Minimum for full holonomic control:** 3 thrusters at 120° spacing around the perimeter (gimballed or variable magnitude), or 4 fixed thrusters if positioned and angled well.
- The spacecraft thruster literature formalises this as T=AF (configuration matrix), giving algebraic conditions for completeness.

### Independent Translation and Rotation

A truly agile body needs to control translation and rotation *independently* (holonomic control). This means the thruster arrangement should be able to produce:

1. **Net force in any direction with zero net torque** (pure translation / strafing)
2. **Net torque with zero net force** (pure rotation / turning in place)

If these are always coupled, the boid can't turn in place or strafe, limiting tactical options like orbiting prey or dodging. Cars are non-holonomic (must turn to change direction); drones/crabs are holonomic.



### Torque and Lever Arms

Rotational agility = torque = force x distance from centre of mass. Thrusters placed far from the CoM with a tangential component give the most rotational authority per unit force. Thrusters clustered near the centre make the boid translationally capable but rotationally sluggish.

### Moment of Inertia

Mass distribution matters: compact boids spin easily; elongated/spread-out boids resist rotation but are more stable. If body shape becomes evolvable, a long thin boid vs. a compact disc will have radically different dynamics with identical thrusters.

### Angular Momentum and Damping

In frictionless 2D, spinning never stops. Without counter-torque capability, evolution will avoid spinning entirely. Angular drag softens this but responsiveness still depends on thruster authority. Ensure enough degrees of freedom for braking rotation.

### Thrust Control Granularity

- **Binary (on/off) fixed thrusters:** 2^n discrete force/torque combinations
- **Variable magnitude:** continuous control within the convex hull of force/torque combinations
- **Gimballing (variable angle):** expands the reachable set further

More continuous control = smoother evolved behaviours, but harder search space.

## Fixed Thruster Arrangements for Full Holonomic Control

### The test

In 2D, full holonomic control means independently commanding 3 degrees of freedom: **Fx** (lateral force), **Fy** (longitudinal force), and **τ** (torque). Each thruster i at body-frame position **(px, py)** firing in direction **(dx, dy)** produces a column vector in force-torque space:

```
ci = (dx, dy, px*dy - py*dx)
         Fx  Fy     torque
```

Because thrusters are **unidirectional** (power ∈ [0, 1], never negative), the attainable set is the **conical hull** of these column vectors — the set of all non-negative linear combinations. For full holonomic control, the columns must **positively span R³**, meaning any (Fx, Fy, τ) target can be achieved with non-negative thruster powers.

Key constraint: to positively span R^n you need at least **n + 1** vectors. So the minimum is **4 unidirectional thrusters** for 3-DOF 2D control. (With bidirectional thrusters that can push both ways, 3 suffices — but that's gimballing.)

### Why some 4-thruster layouts fail

**Opposed pairs through CoM:** Two pairs (e.g. ±X and ±Y) passing through the centre of mass. Every thruster has zero lever arm → zero torque. The columns are all (Fx, Fy, 0) — they only span a 2D plane in force-torque space. Full translation, zero rotation. **Not holonomic.**

**All thrusters on one side:** If all thrusters produce torque of the same sign (e.g. all CCW), the attainable torque is only non-negative. Cannot produce CW rotation. **Not holonomic.**

**Collinear force-torque columns:** If the column vectors, viewed as points in (Fx, Fy, τ) space, all lie in a plane through the origin, they can't span the full 3D volume. This happens when thruster positions and angles are too "regular" — e.g. 4 thrusters all firing tangentially in the same rotational sense.

### 4-thruster arrangements that work

**A. Pinwheel (alternating tangential, 90° spacing)**

Four thrusters on the perimeter at compass points, each firing tangentially, alternating CW/CCW:

```
         (0, r) firing (+1, 0)    → column: (+1, 0, -r)    [CW torque]
(-r, 0) firing (0, +1)           → column: (0, +1, +r)    [CCW torque]
         (0, -r) firing (-1, 0)   → column: (-1, 0, -r)    [CW torque]
(+r, 0) firing (0, -1)           → column: (0, -1, +r)    [CCW torque]
```

These 4 columns positively span R³. To get pure +τ, fire the two CCW thrusters equally — their Fx and Fy cancel, torques add. To get pure +Fx, combine thrusters to cancel Fy and τ. Classic spacecraft RCS arrangement.

**B. X-configuration (4 canted thrusters at corners)**

Four thrusters at (±r, ±r), each firing inward-and-tangential at ~45° to the radius. Produces 4 columns with mixed Fx, Fy, τ components that span the space. Common in satellite designs (the "X" or "diamond" layout).

**C. Longitudinal + differential pair**

Two centreline thrusters (fore/aft, zero torque) plus two off-centre thrusters that provide opposing torques:

```
rear:       (0, -r) firing (0, +1)    → (0, +1, 0)
front:      (0, +r) firing (0, -1)    → (0, -1, 0)
left-rear:  (-d, -r) firing (+1, 0)   → (+1, 0, +dr) [CCW]
right-rear: (+d, -r) firing (-1, 0)   → (-1, 0, -dr) [CW]
```

This is close to wildboids' original 4-thruster layout. It **fails the full holonomic test**: the centreline thrusters give ±Fy with zero τ, and the differential pair give ±Fx with ±τ always coupled. You can get pure Fy and pure τ (fire both differential thrusters equally → Fx cancels, torques cancel... wait, no — they produce *opposing* torques, so equal firing gives zero τ and net Fx = 0). To get pure +Fx, fire left-rear only, but that also gives +τ. You can't get pure Fx without torque. **Not fully holonomic** — lateral force is always coupled to rotation.

### 6-thruster arrangements that work

Adding thrusters makes full holonomic control much easier to achieve. The extra columns provide redundancy, making it almost hard to *fail* if the layout has reasonable diversity.

**D. Longitudinal + differential + strafe pair (current wildboids layout)**

```
rear:         (0, -0.5) firing (0, +1)           → (0, +1, 0)        [pure forward]
left_rear:    (-0.35, -0.6) firing (0.92, 0.39)  → (+0.92, +0.39, +0.42)  [CCW + forward-right]
right_rear:   (+0.35, -0.6) firing (-0.92, 0.39) → (-0.92, +0.39, -0.42)  [CW + forward-left]
front:        (0, +0.5) firing (0, -1)            → (0, -1, 0)        [pure backward]
strafe_left:  (0, 0) firing (-1, 0)               → (-1, 0, 0)        [pure left]
strafe_right: (0, 0) firing (+1, 0)               → (+1, 0, 0)        [pure right]
```

**This is fully holonomic.** The strafe pair fills the gap the 4-thruster version had:
- **Pure Fx:** fire strafe_left or strafe_right alone (zero τ, zero Fy)
- **Pure Fy:** fire rear or front alone (zero τ, zero Fx)
- **Pure +τ:** fire left_rear, cancel its Fx with strafe_left, cancel its Fy with front → net pure CCW torque
- **Pure -τ:** symmetric using right_rear

The 6 columns comfortably positively span R³ with redundancy.

**Note on the torque thrusters' force "contamination":** The current left/right_rear thrusters produce mixed force+torque columns — e.g. left_rear gives (+0.92, +0.39, +0.42), so firing it alone pushes the boid forward-right *and* rotates it CCW. Pure torque requires a 3-thruster combination to cancel out the unwanted Fx and Fy. This works, but it's inefficient: energy is spent on force that gets cancelled by counter-thrust.

Since we already have 4 thrusters covering the pure force axes (rear/front for ±Fy, strafe pair for ±Fx), we could reposition the torque pair to produce **pure or near-pure torque as a pair**, freeing them from double duty. Two options:

**Option 1: Symmetric tangential pair.** Place two thrusters at equal distance from CoM, firing in opposite tangential directions:

```
torque_ccw:  (-0.5, 0.0) firing (0, +1)  → (0, +1, +0.5)
torque_cw:   (+0.5, 0.0) firing (0, -1)  → (0, -1, +0.5)
```

Fired together at equal power: Fx cancels, Fy cancels, torques *add* → pure CCW rotation. Fired individually, each still contaminates Fy, but the pair gives clean torque. To get pure CW, swap which one fires (or reposition to allow it — see Option 2).

**Option 2: Opposed tangential pair (pure torque per thruster pair, either direction).** Two thrusters at the same position but firing in opposite directions, offset from CoM:

```
torque_ccw:  (0, -0.5) firing (+1, 0)  → (+1, 0, +0.5)
torque_cw:   (0, -0.5) firing (-1, 0)  → (-1, 0, -0.5)
```

Wait — these have opposite torque signs but also opposite Fx. Fired together: Fx cancels, τ cancels too. That's no good. Better: place them on opposite sides:

```
torque_ccw:  (-0.5, 0) firing (0, +1)  → (0, +1, +0.5)
torque_cw:   (+0.5, 0) firing (0, +1)  → (0, +1, -0.5)
```

Fired together at equal power: Fx = 0, Fy = +2 (not zero — contaminates Fy). Pure torque as a pair is only achievable if the force components cancel, which requires the forces to point in *opposite* directions. But then the torques also oppose unless the lever arms differ. This is the fundamental constraint: **no single fixed thruster can produce pure torque** (it always produces force too), and getting pure torque from a pair requires their net forces to cancel while their torques add.

**The cleanest pair for pure torque** is two thrusters at the same radius, diametrically opposite, both firing tangentially in the *same rotational sense*:

```
torque_a:  (-r, 0) firing (0, +1)  → (0, +1, +r)   [CCW, pushes forward]
torque_b:  (+r, 0) firing (0, -1)  → (0, -1, +r)   [CCW, pushes backward]
```

Fired together: Fy cancels, Fx = 0, τ = +2r. **Pure CCW torque.** For CW, you need a second pair (or reverse the arrangement). This is exactly the pinwheel principle applied to a pair.

**Practical upshot for the current 6-thruster layout:** The mixed force+torque columns aren't a *problem* for holonomic control (the math works), but they mean the boid wastes energy achieving pure rotation. If the torque thrusters were repositioned to fire tangentially from the widest points (e.g. (±0.5, 0) firing (0, ±1)), the boid could rotate without fighting its own translation thrusters. Whether this matters depends on the energy cost regime — if thrust is cheap, the waste is negligible; if expensive, cleaner torque = more efficient turning = evolutionary advantage.

**However**, there's a counterargument for keeping the current "dirty" torque thrusters: they give NEAT **more interesting combinations to discover**. A thruster that does two things at once (turn + push) is a richer building block than one that does exactly one thing. Evolution might find that the coupling between rotation and forward thrust is *useful* — e.g. a "power turn" where the boid simultaneously rotates and accelerates. Pure-function thrusters are easier to reason about but may constrain the emergent behaviour vocabulary.

**E. Perimeter ring (6 thrusters at 60° intervals)**

Six thrusters equally spaced around the body edge, each firing tangentially (alternating CW/CCW):

```
Positions at angles 0°, 60°, 120°, 180°, 240°, 300° on a circle of radius r.
Each fires tangentially, alternating direction.
```

Maximally symmetric, highly redundant, every direction equally accessible. The "ideal" arrangement if there's no preferred forward direction. Less natural for a boid that has a clear front/back.

**F. Three opposed pairs**

Three pairs through or near the CoM, each pair aligned to a different axis: one ±Fx pair, one ±Fy pair, one torque pair (two off-centre thrusters producing opposing torques). Straightforward, orthogonal control axes, easy to reason about.

### Summary table

| Arrangement | Thrusters | Holonomic? | Notes |
|---|---|---|---|
| Opposed pairs through CoM | 4 | No | Zero torque capability |
| Longitudinal + differential (old wildboids) | 4 | No | Lateral force coupled to torque |
| Pinwheel (alternating tangential) | 4 | Yes | Minimum viable; no preferred axis |
| X-configuration (canted corners) | 4 | Yes | Minimum viable; compact |
| Longitudinal + differential + strafe (current wildboids) | 6 | **Yes** | Redundant; clear role per thruster |
| Perimeter ring (60° intervals) | 6 | Yes | Maximally symmetric; no preferred direction |
| Three opposed pairs | 6 | Yes | Orthogonal control axes; easy to analyse |

## Design Implications for Wildboids

### User-Defined Thruster Setups

To explore movement space before making thrusters fully evolvable, provide preset thruster configurations that span different movement strategies:

- **Symmetric 4-thruster (current):** balanced baseline
- **Wide-stance rear + lateral:** car-like forward bias with strafing ability
- **Perimeter ring (3 or 4):** full holonomic control
- **Asymmetric layouts:** enable specialised manoeuvres (fast jink in one direction)

Each preset occupies a different region of the force-torque space, letting us observe what behaviours evolution discovers under different physical constraints.

### Energy Cost Structure Shapes Movement Ecology

Energy costs matter as much as physics for shaping evolved behaviour:

| Cost regime | Expected evolved strategy |
|---|---|
| Cheap rotation, expensive translation | Turn-then-thrust (car-like) |
| Both cheap | Strafing, complex evasion |
| Expensive sustained thrust, cheap bursts | Darting, lunging |

### Predator/Prey Balance

For interesting co-evolution, ensure the movement space allows both:
- **Fast but wide-turning** builds (outrun strategy)
- **Slow but agile** builds (outturn strategy)

If only one dominant strategy is physically viable, dynamics collapse.

### Asymmetry as Evolvable Advantage

Symmetric configurations are a local optimum. Slightly asymmetric layouts can enable manoeuvres symmetric boids can't match. If thruster layout becomes evolvable, the genome should be able to encode asymmetric placements.

## Key References

### Evolved Creatures / Artificial Life

- **Karl Sims, "Evolving Virtual Creatures" (1994)** — Seminal work on co-evolving morphology and neural control under real physics. [Paper](https://www.karlsims.com/papers/siggraph94.pdf) | [Overview](https://www.karlsims.com/evolved-virtual-creatures.html)
- **ALIEN Project** — CUDA 2D artificial life simulator with physics-based organisms, neural controllers, and emergent predator/prey ecosystems. Very close to this project's goals. [Site](https://www.alien-project.org/) | [GitHub](https://github.com/chrxh/alien)
- **Hartl et al., "Neuroevolution of Decentralized Decision-Making in N-Bead Swimmers" (2024/2025)** — Neuroevolution for locomotion; decentralised control policies generalise across morphologies. [arXiv](https://arxiv.org/html/2407.09438) | [Nature](https://www.nature.com/articles/s42005-025-02101-5)
- **"Convergent Evolution in Silico" (eLife, 2024)** — Evolves soft-bodied robots; bilateral symmetry and modularity emerge convergently. [eLife](https://elifesciences.org/reviewed-preprints/87180)

### Thruster Configuration Theory

- **Sanchez-Pena & Servidia (2002)** — Configuration matrix formalism (T=AF) for analysing thruster layouts. 4 thrusters suffice for 3-DOF 2D control. [ResearchGate](https://www.researchgate.net/publication/3003530_Thruster_design_for_positionattitude_control_of_spacecraft)
- **Servidia & Sanchez-Pena (2005)** — Attainable torque/force set visualisation and control allocation. [IEEE](https://ieeexplore.ieee.org/document/1393143/)
- **TPODS Satellite Testbed (2024)** — Compares 4-thruster configs (X, H, offset-H) on 2D air-bearing table. Directly maps to 2D boid problem. [arXiv](https://arxiv.org/html/2409.09633v1/)
- **Thruster Configuration Optimisation for Underwater Robots (2024)** — Uses GAs to optimise thruster placement for maximum agility. Methodology parallels what boid evolution does naturally. [Ocean Engineering](https://www.sciencedirect.com/science/article/abs/pii/S0029801824017839)

### Steering Behaviours (Context)

- **Craig Reynolds, "Steering Behaviors for Autonomous Characters" (GDC 1999)** — Classic composable steering forces. Uses point mass with no rotational inertia — exactly the limitation our physics model goes beyond. [Site](https://www.red3d.com/cwr/steer/) | [Boids](https://www.red3d.com/cwr/boids/)
- **Daniel Shiffman, "Nature of Code" Ch.5** — Accessible implementation of Reynolds' behaviours. [Book](https://natureofcode.com/autonomous-agents/)

### NEAT + Locomotion

- **Tibermacine & Djedi (2018)** — NEAT for locomotion in physics sims with varying viscosity. NEAT outperforms standard GAs. [ResearchGate](https://www.researchgate.net/publication/310509452_Artificial_evolution_using_neuroevolution_of_augmenting_topologies_NEAT_for_kinetics_study_in_diverse_viscous_mediums)

## Next Steps

- [ ] Analyse current 4-thruster layout's force-torque space coverage
- [ ] Design 2-3 alternative preset thruster configs spanning different movement strategies
- [ ] Consider making energy cost structure configurable per experiment
- [ ] Evaluate whether moment of inertia / body shape should become evolvable parameters
