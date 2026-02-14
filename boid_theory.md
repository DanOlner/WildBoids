# Physically-Grounded Boid Locomotion

CLaude-generated doc. Prompts/replies are [here](llm_convos/2026-01-20_1706_Read_the_CLAUDEmd_file_and_then.md) (latter half).

---

**Purpose:** Explore replacing Reynolds-style pseudo-physics with a thrust-based physical model for Wild Boids 2.0, where boids have actual thrusters that produce forces and torques on a rigid body. This creates a tighter coupling between movement and energy, and prevents boids from "hacking" non-physical movement rules.

---

## The Problem with Pseudo-Physics

Reynolds' original boids (and the 2007 Wild Boids) use a **kinematic** model: each boid's velocity is directly modified by steering behaviors. There's no concept of mass, inertia, or force — a boid can change direction instantly (limited only by arbitrary damping parameters). This creates exploitable loopholes:

- Boids can evolve to make **physically impossible** turns (instant reversal)
- There's no energy cost to movement, so evolved strategies face no efficiency pressure
- Without inertia, boids lack the momentum-based movement that makes real organisms interesting (overshooting, drifting, banking)
- Evolved behaviors can "hack" the gap between the pseudo-physics and what looks like natural motion

A thrust-based model grounds movement in physics, meaning evolved strategies must respect real constraints — and those constraints become part of what evolution shapes.

---

## Biological Locomotion Projected to 2D: Is a Thruster Model Reasonable?

Before committing to a specific model, it's worth asking: when we squash the 3D reality of bird flight or fish swimming onto a 2D plane, what are the fundamental control axes, and does a "front/back/two lateral thrusters" arrangement capture them?

### What Animals Actually Control

Across very different body plans, animal locomotion in the horizontal plane reduces to a surprisingly small number of effective degrees of freedom:

- **Forward thrust** — the primary propulsive force. In fish this comes from the [body and caudal fin (BCF mode)](https://en.wikipedia.org/wiki/Fish_locomotion); in birds from wing downstrokes; in insects from wing beat asymmetries.
- **Yaw torque** (steering) — rotation about the vertical axis. Fish achieve this via [differential pectoral fin action](https://bpb-us-e1.wpmucdn.com/sites.harvard.edu/dist/6/58/files/2022/03/Lauder.Drucker.2004.pdf), birds via tail twisting and differential wing drag, insects via [independent modulation of left/right wing stroke](https://pmc.ncbi.nlm.nih.gov/articles/PMC2654101/).
- **Braking** — deceleration. Fish spread their pectoral fins to increase drag; birds flare wings and tail. This is always costly and less powerful than forward thrust.

Research on insect flight dynamics shows that even Drosophila, with its complex wing kinematics, effectively controls [just three degrees of freedom](https://pmc.ncbi.nlm.nih.gov/articles/PMC2654101/) — axial force, side force, and yaw torque — and these map naturally to our 2D plane. Horizontal-plane models of insect locomotion similarly [restrict dynamics to three DOF](https://pubmed.ncbi.nlm.nih.gov/19660474/): translation in x, translation in y, and yaw rotation.

### The Thrust-vs-Manoeuvrability Trade-off Is Real

Fish biology provides a clear analogy for our model. Fish swimming divides into two broad modes ([reviewed here](https://researchportal.hw.ac.uk/en/publications/review-of-fish-swimming-modes-for-aquatic-locomotion/)):

| Mode | Mechanism | Good For | Body Shape |
|------|-----------|----------|------------|
| **BCF** (body-caudal fin) | Whole-body undulation, tail thrust | Speed, cruising, acceleration | Streamlined, narrow tail |
| **MPF** (median-paired fin) | Pectoral/dorsal fin sculling | Manoeuvrability, hovering, precision | Deep, laterally compressed |

The key insight: **there's a real biological trade-off between thrust and manoeuvrability**, driven by body morphology. Fast fish (tuna, mackerel) sacrifice turning ability; manoeuvrable fish (butterflyfish, wrasses) sacrifice speed. This is exactly the kind of trade-off that a thrust-based model can capture, where boids must choose between powering forward and rotating — and can't do both maximally at once.

### Existing Simplified Locomotion Models

Robotics has well-studied 2D locomotion models that are relevant here:

**The [unicycle model](http://faculty.salina.k-state.edu/tim/robotics_sg/Control/kinematics/unicycle.html)** — the simplest useful 2D locomotion: one forward velocity v and one angular velocity ω. State update: `dx = v·cos(θ)`, `dy = v·sin(θ)`, `dθ = ω`. This is essentially what Reynolds' boids use (kinematic, no forces). Clean but non-physical.

**The [differential drive model](https://ucr-robotics.readthedocs.io/en/latest/tbot/moRbt.html)** — two independently driven wheels. Left/right wheel velocities produce both forward motion and turning. Reduces to the unicycle model mathematically, but the two-wheel framing maps nicely to "two side thrusters" producing differential torque. Still kinematic.

**[Braitenberg vehicles](https://en.wikipedia.org/wiki/Braitenberg_vehicle)** — Valentino Braitenberg's 1984 thought experiment: creatures with sensors directly wired to two motors (left and right wheels). Despite radical simplicity, [ipsilateral vs contralateral wiring and excitatory vs inhibitory coupling](https://pmc.ncbi.nlm.nih.gov/articles/PMC7525016/) produce surprisingly complex emergent behaviors: attraction, avoidance, aggression, love. This is a powerful precedent for Wild Boids — it shows that **simple sensor→actuator mappings can produce rich behavior**, especially when evolved rather than hand-designed.

**Force-based rigid body** (our proposed model) — adds mass, inertia, and drag on top of the differential-drive concept. The thrusters produce forces rather than directly setting velocities, so momentum, overshoot, and energy cost all emerge naturally.

### How the 4-Thruster Model Maps to Biology

| Thruster | Biological Analogue | Function |
|----------|--------------------|---------|
| **Rear** (forward thrust) | Caudal fin / wing downstroke | Primary propulsion |
| **Front** (braking) | Spread pectoral fins / wing flare | Deceleration (costly, weaker than propulsion) |
| **Left-rear** (CW torque) | Right pectoral fin / left wing drag | Steer right |
| **Right-rear** (CCW torque) | Left pectoral fin / right wing drag | Steer left |

This captures the essential biological reality:
- **Forward thrust dominates** — animals are built to move in the direction they face
- **Turning is separate from propulsion** — you rotate, then thrust, or combine them at reduced efficiency
- **Braking is expensive and weak** — most animals decelerate mainly through drag, with active braking as a supplementary option
- **No lateral movement** — animals (with a few exceptions like cuttlefish) can't strafe; they must rotate first

### Should We Consider Alternatives?

**3-thruster model (drop the front thruster):** More biologically accurate in some ways — most animals rely on passive drag for deceleration rather than active reverse thrust. Would force evolution to manage speed purely through thrust modulation and drag. Simpler genome (3 outputs instead of 4). Worth considering as the default.

**Differential thrust only (2 thrusters, left and right at rear):** Essentially a Braitenberg vehicle with physics. Forward motion comes from firing both; turning from differential firing. Very elegant, maps directly to the differential drive model, and has the smallest possible genome footprint. The limitation: no way to thrust backward at all, and maximum forward thrust also means no turning authority (both thrusters maxed). This constraint might actually be *interesting* for evolution.

**Variable geometry (evolvable thruster positions):** Let the genome encode where thrusters sit on the body. Different moment arms would produce different turning characteristics. Fascinating but adds complexity — probably a later extension.

> **What's a moment arm?** When a force is applied to a rigid body away from its center of mass, it produces both linear acceleration *and* rotation (torque). The **moment arm** is the perpendicular distance between the line of the force and the center of mass. Torque = force × moment arm. So the same thruster force produces *more* rotation when it's further from the center of mass, and *zero* rotation if the force line passes directly through it. This is why we place the side thrusters near the rear of the boid: they're far from the center of mass, giving them a long moment arm, so even a small lateral force produces strong rotation. If they were at the center, they'd push the boid sideways (translation) but wouldn't turn it at all. In the torque equation from the physics section below (`τ = r.x × F.y − r.y × F.x`), the **r** vector *is* the moment arm — it's the displacement from center of mass to thruster position. Longer r = more torque per unit force.

**Continuous thrust direction (single steerable thruster):** One thruster whose angle can rotate. More like a rudder + propeller. Simpler output (thrust power + thrust angle) but loses the emergent quality of combining multiple fixed thrusters.

### Assessment

The 4-thruster model is a reasonable and defensible simplification of biological locomotion in 2D. It captures the three essential control axes (forward thrust, braking, yaw) that animal locomotion studies consistently identify. The Braitenberg vehicle literature gives confidence that simple sensor→thruster mappings will produce rich emergent behavior when evolved. And the fish BCF/MPF trade-off literature confirms that the speed-vs-manoeuvrability tension built into the model has real biological grounding.

The strongest alternative is the **3-thruster model** (dropping the front brake), which is arguably more biologically honest and reduces the genome. The **2-thruster differential** model is the most elegant but may be too constrained. A sensible approach: **start with 3 or 4 thrusters and treat the count as an early design decision to test empirically**.

---

## Proposed Model 1: Thruster-Based 2D Rigid Body

### Core Idea

Each boid is a **2D rigid body** with a small number of fixed thrusters. The boid's "brain" (the evolved sensory-behavioral system) doesn't directly set velocity — it can only decide which thrusters to fire and at what power. All movement emerges from the resulting forces and torques.

### Thruster Layout

A simple, biologically-inspired arrangement using **4 thrusters**:

```
          [Front Thruster]
               ▲
               |
    ┌──────────┼──────────┐
    │          (●)         │   (●) = center of mass
    └──────────┼──────────┘
         ◄─[L]─┼─[R]─►
               |
               ▼
         [Rear Thruster]
```

1. **Front thruster** — fires backward (produces braking / reverse thrust)
2. **Rear thruster** — fires forward (main propulsion, like a tail fin or jet)
3. **Left-rear thruster** — fires rightward (produces counter-clockwise torque)
4. **Right-rear thruster** — fires leftward (produces clockwise torque)

The side thrusters are positioned near the rear so they produce **torque** (rotation) rather than pure lateral translation. This mimics how fish use their tail and pectoral fins, or how spacecraft use attitude thrusters.

#### Why This Layout?

- **Asymmetric by design**: forward thrust (rear thruster) is the primary driver, as with most animals. This means boids naturally move in the direction they face.
- **Rotation via side thrusters**: placing them at the back maximizes the moment arm, giving efficient rotation with small forces.
- **Front thruster for braking**: allows evolved deceleration strategies, but at a cost (you burn energy to slow down, just like in reality).
- **No lateral thrust**: boids can't strafe — they must rotate to change direction, producing realistic turning arcs.

### Alternative Layouts Worth Considering

- **3 thrusters** (no front thruster): simpler, but boids can only decelerate via drag. Could be more interesting evolutionarily — prey might evolve to always maintain speed rather than brake.
- **6 thrusters** (add two forward side thrusters): allows differential braking and tighter turns, but more genes to evolve.
- **Asymmetric/evolvable positions**: let evolution determine where thrusters are placed on the body. More complex but could produce fascinating specialisation.

---

## The Physics

### 2D Rigid Body Equations

Each boid has these state variables:

| Variable | Symbol | Description |
|----------|--------|-------------|
| Position | **p** = (x, y) | World-space location |
| Velocity | **v** = (vx, vy) | Linear velocity |
| Angle | θ | Heading (radians) |
| Angular velocity | ω | Rotation rate (rad/s) |
| Mass | m | Determines acceleration from force |
| Moment of inertia | I | Determines angular acceleration from torque |

### Force and Torque from Thrusters

Each thruster i has:
- A **position** relative to center of mass: **r**_i = (rx, ry)
- A **direction** it fires in (body-local frame): **d**_i
- A **power level**: p_i ∈ [0, 1] (controlled by the boid's brain)
- A **maximum thrust**: F_max

The force from thruster i in world frame:

```
F_i = p_i × F_max × rotate(d_i, θ)
```

The torque from thruster i ([cross product in 2D](https://www.toptal.com/game/video-game-physics-part-i-an-introduction-to-rigid-body-dynamics)):

```
τ_i = r_i.x × F_i.y − r_i.y × F_i.x
```

### Integration (per timestep dt)

```
Total force:     F_total = Σ F_i + F_drag
Total torque:    τ_total = Σ τ_i + τ_rotational_drag

Linear:          v += (F_total / m) × dt
                 p += v × dt

Angular:         ω += (τ_total / I) × dt
                 θ += ω × dt
```

Where **I** for a simple elliptical boid shape can be approximated as:

```
I = m × (a² + b²) / 4     (for an ellipse with semi-axes a, b)
```

Or for a simple rectangle: `I = m × (w² + h²) / 12`

### Drag

Essential to prevent infinite acceleration and to create a terminal velocity:

```
F_drag = −c_d × |v| × v     (quadratic drag, physically realistic)
τ_rotational_drag = −c_r × ω  (linear rotational damping)
```

The drag coefficient c_d is a key parameter — it determines the boid's terminal velocity for a given thrust, and creates the natural deceleration when thrusters aren't firing.

---

## Energy System

This is where the thrust model really pays off for evolution.

### Energy Budget

Each boid has an **energy reserve** E that:
- **Decreases** when thrusters fire: `ΔE = −Σ(p_i × F_max × efficiency_cost) × dt`
- **Decreases** with a baseline metabolic rate: `ΔE = −metabolism × dt`
- **Increases** when eating (predators eat prey; prey eat food/resources)

### Why Energy Matters for Evolution

With energy costs, evolution faces real trade-offs:

| Strategy | Energy Cost | Fitness Effect |
|----------|------------|----------------|
| Always full thrust | Very high | Fast but starves quickly |
| Constant turning/spinning | High | Wastes energy on rotation |
| Cruise and burst | Moderate | Efficient predator strategy |
| Minimal movement | Low | Saves energy but easier to catch/can't catch |

This naturally prevents the "hacking" problem from the 2007 version. A boid can't evolve to vibrate wildly or make impossible turns because:
1. All turns cost energy (thruster fuel)
2. Inertia means you can't instantly reverse
3. Energy depletion means inefficient strategies die out

### Connection to Tu & Terzopoulos' Artificial Fish

This approach echoes the [Artificial Fishes](https://dl.acm.org/doi/10.1145/192161.192170) work (SIGGRAPH '94) by Tu and Terzopoulos, which modeled fish with physics-based locomotion, internal muscle actuators, and energy-efficient movement patterns. Their key insight: **natural locomotion patterns are energetically efficient**, so optimizing for energy conservation naturally produces realistic movement. Our thrust model achieves something similar — evolution should discover that smooth, efficient thrust patterns outperform wasteful ones.

---

## What the Boid Brain Controls

The evolved sensory/behavioral system's **output** is now simply:

```
Output vector: [front_thrust, rear_thrust, left_thrust, right_thrust]
Each value: 0.0 to 1.0
```

This is a clean, low-dimensional output that the genetic system maps to. The sensory system (from [evolution_theory.md](evolution_theory.md)) detects neighbors and threats, and the behavioral genome determines how sensor inputs map to these four thrust values.

### Mapping Sensors to Thrusters

With the direct parameter encoding approach from evolution_theory.md, each sensor could have evolved weights for each thruster:

```
Per sensor:
  - perception_angle, perception_radius (where to look)
  - thrust_weights[4] (how detection in this sensor maps to each thruster)
```

So if a prey boid's "predator-ahead" sensor fires, the evolved weights might produce: `[0, 1.0, 0.3, 0]` — full rear thrust (flee forward) plus some left thrust (veer right). This mapping is entirely evolved, not hand-designed.

---

## Implications for Evolution

### What Changes from the 2007 Approach

| Aspect | 2007 Wild Boids | Thrust-Based Model |
|--------|----------------|-------------------|
| Movement control | Direct velocity/angle | Thruster power levels |
| Physics | Pseudo (instant turns) | Real (force → acceleration → velocity) |
| Energy | None | Thrust costs energy |
| Turning | Instant, arbitrary | Requires torque, has inertia |
| Speed | Directly set | Emerges from thrust vs drag |
| Genome output | Direction + speed | 4 thrust values |
| Exploit prevention | None | Physics constraints |

### New Evolutionary Pressures

The thrust model introduces pressures that didn't exist before:

1. **Energy efficiency**: wasteful movers die. Evolution should discover coast-and-burst patterns, efficient turning, minimal braking.

2. **Momentum management**: can't stop on a dime. Prey must anticipate, predators must plan intercept courses rather than pure pursuit.

3. **Rotation economy**: spinning wastes energy and time. Boids should evolve to rotate only as much as needed.

4. **Speed-manoeuvrability trade-off**: a boid moving fast has more kinetic energy and more inertia — harder to turn. This creates a real physical trade-off that evolution must navigate.

### Predator vs Prey Asymmetry

Could give predators and prey **different physical parameters** (not just different brains):

- **Predators**: higher max thrust, higher mass (harder to turn), higher metabolism (must eat frequently)
- **Prey**: lower max thrust, lower mass (nimble), lower metabolism (can survive longer)

Or: keep physics identical and let behavioral evolution alone produce the asymmetry.

---

## Implementation Considerations

### Computational Cost

The thrust model adds per-boid per-frame:
- 4 thruster force calculations (trivial: multiply + rotate)
- 1 torque sum
- 1 force sum
- 2 integration steps (linear + angular)
- 1 drag calculation

This is negligible compared to the O(n) neighbor lookups. The physics adds maybe 20 floating-point operations per boid per frame — nothing compared to spatial queries.

### Tuning Parameters

These are **simulation constants** (not evolved):

| Parameter | Purpose | Starting Value |
|-----------|---------|---------------|
| `max_thrust` | Maximum force per thruster | Tune to feel |
| `mass` | Boid mass | 1.0 (normalize) |
| `drag_coefficient` | Determines terminal velocity | 0.1–0.5 |
| `rotational_drag` | Prevents infinite spinning | 0.3–0.8 |
| `energy_per_thrust` | Energy cost of firing | Tune so boids last ~minutes |
| `metabolism` | Baseline energy drain | Small fraction of thrust cost |
| `moment_of_inertia` | Rotation resistance | Derived from shape + mass |

### Visualisation Opportunities

The thrust model gives nice visual feedback:
- Show thruster flames/trails when firing
- Boid orientation is meaningful (not just a velocity arrow)
- Turning arcs visible and physically grounded
- Energy bar per boid (or color-coded)

---

## Open Questions

1. **Should thruster positions be evolvable?** Would let evolution discover optimal body plans, but adds genome complexity.

2. **Should mass be evolvable?** Heavier boids are harder to turn but harder to push around. Could create interesting predator/prey dynamics.

3. **Continuous vs binary thrusters?** The 0-1 continuous range is more expressive, but on/off thrusters (like real attitude control systems) would be simpler to evolve and prevent fine-tuning exploits.

4. **What about drag asymmetry?** Real fish/birds have less drag moving forward than sideways. Could add a directional drag coefficient.

5. **Collision physics?** With real mass and velocity, boid-boid collisions could be physically simulated (elastic/inelastic). Predators could "ram" prey.

---

## Proposed Model 2: Genome-Defined Thruster Layout

### The Idea

Rather than hardcoding where thrusters sit (front, rear, left-rear, right-rear), let the **genome define thruster positions and angles** on the boid's body. Each thruster is described by just three numbers:

```
Per thruster in genome:
  - position_along_body:  -1.0 (nose) to +1.0 (tail)
  - position_across_body: -1.0 (left edge) to +1.0 (right edge)
  - firing_angle:          0 to 2π (direction the thruster pushes, in body-local frame)
```

These are set at birth (inherited from parents, subject to mutation) and **fixed for the boid's lifetime** — no mid-life reconfiguration. The physics engine doesn't care where thrusters are; it already computes force and torque from arbitrary positions (the **r**_i and **d**_i vectors in the physics section above). So this adds zero runtime cost to the simulation loop.

### How Much Genome Complexity Does This Add?

For a fixed N-thruster layout:

| Thruster count | Fixed layout (Model 1) | Genome-defined (Model 2) |
|---------------|----------------------|------------------------|
| **Genes per thruster** | 0 (hardcoded) | 3 (x-pos, y-pos, angle) |
| **2 thrusters** | 0 extra genes | 6 extra genes |
| **3 thrusters** | 0 extra genes | 9 extra genes |
| **4 thrusters** | 0 extra genes | 12 extra genes |

For context, the sensory genome from [evolution_theory.md](evolution_theory.md) already has ~6 genes per sensor × 3-5 sensors = 18-30 genes. Adding 6-12 thruster-position genes is a **modest increase** (~30-50% more genome), not a doubling.

The brain output size stays the same: N thrust power values in [0, 1], one per thruster. The sensor→thruster weight matrix stays the same size too. The only thing that changes is that thruster positions/angles come from the genome rather than being constants.

### What Changes at Runtime?

Almost nothing. Here's what happens at boid creation vs. per-frame:

**At birth (once):**
```
For each thruster gene (position_along, position_across, firing_angle):
    // Map normalised gene values to body coordinates
    r_i.x = position_across × body_half_width
    r_i.y = position_along × body_half_length
    d_i   = (cos(firing_angle), sin(firing_angle))
```

**Per frame (unchanged from Model 1):**
```
For each thruster i:
    F_i = power_i × F_max × rotate(d_i, θ)        // force in world frame
    τ_i = r_i.x × F_i.y − r_i.y × F_i.x           // torque from this thruster
Sum forces, sum torques, integrate. Done.
```

The per-frame loop is **identical**. The only difference is where **r**_i and **d**_i come from: a constant in Model 1, a genome lookup in Model 2. Since these are read once per thruster per frame (not computed), there is genuinely no measurable performance difference.

### The Differential-Drive Configuration Falls Out Naturally

This is the key payoff of Model 2: different thruster configurations aren't separate "models" that need separate code paths — they're just different genome values. Want to test the 2-thruster Braitenberg-style differential drive? Set N=2 and let evolution find the layout. In practice it would likely converge on something like:

```
Thruster 0: position = (−0.3, +0.8), angle = 270°   (left-rear, fires forward)
Thruster 1: position = (+0.3, +0.8), angle = 270°    (right-rear, fires forward)
```

Both fire forward → boid moves ahead. Left fires stronger → boid turns right (more thrust on the left side creates clockwise torque). Right fires stronger → boid turns left. Neither fires → boid coasts on momentum and drag.

But evolution might *not* converge on that symmetric layout. It might discover:
- **Asymmetric placements** where one thruster is further from center (longer moment arm → more turning authority on that side)
- **Angled thrusters** that produce combined forward + rotational force in a single firing
- **One thruster near the nose** angled sideways for quick pivoting
- **Predators and prey with different layouts** — predators might evolve a rear-biased layout for straight-line speed, prey a more central layout for agile turning

### What Configurations Could Emerge?

Some plausible evolved layouts (for N=3):

```
"The speedboat"              "The spinner"              "The asymmetric"
   ┌─────────┐                 ┌─────────┐                ┌─────────┐
   │         │                 │  ↗   ↖  │                │    ↑    │
   │   (●)   │                 │   (●)   │                │   (●)   │
   │         │                 │         │                │  ↗      │
   │  ← ↑ → │                 │    ↑    │                │      ↑  │
   └─────────┘                 └─────────┘                └─────────┘
Two rear side + one rear       Two forward-angled +        Unequal positions:
centre = fast, stable          one rear = pivot + go       natural turning bias
```

### Constraining the Design Space

Unconstrained thruster placement could produce degenerate configurations (all thrusters at the same point, or all firing in the same direction). Some options:

**No constraints (purist approach):** Let evolution discover that bad layouts die. A boid with all thrusters pointing the same way can only accelerate in one direction — it'll be eaten/starve. This is the simplest to implement and most "evolutionary" in spirit, but might slow down early evolution while bad layouts are culled.

**Soft constraints via genome structure:** Rather than fully free placement, define thruster positions relative to zones:

```
Thruster 0: always in rear half    (position_along ∈ [0, 1])
Thruster 1: always in rear half    (position_along ∈ [0, 1])
Thruster 2: anywhere               (position_along ∈ [-1, 1])
```

This nudges evolution toward having at least some rear propulsion while still allowing creativity.

**Body-shape constraints:** Thrusters must sit on the body's perimeter rather than anywhere inside it. For an elliptical body, the position genes would define a point on the ellipse boundary. This is more biologically plausible (fins are on the outside) and prevents degenerate stacking at the center of mass.

### Comparing Model 1 and Model 2

| Aspect | Model 1 (Fixed Layout) | Model 2 (Genome-Defined) |
|--------|----------------------|------------------------|
| Implementation effort | Simpler | Slightly more (birth-time gene→position mapping) |
| Runtime cost | Identical | Identical |
| Genome size | N power weights per sensor | N power weights + 3N position/angle genes |
| Design flexibility | One layout, test by recoding | Any layout, test by changing initial genes |
| Evolutionary richness | Brain only | Brain + body plan |
| Interpretability | Easy (you know what each thruster does) | Harder (must inspect evolved layouts) |
| Risk of degenerate configs | None | Some (mitigated by selection pressure or soft constraints) |
| Enables differential-drive test | Requires separate code | Just set N=2, let it evolve |

### Recommendation

**Model 2 adds very little complexity for a lot of evolutionary potential.** The physics code is unchanged. The per-frame loop is unchanged. The only additions are:

1. 3 extra genes per thruster in the genome
2. A one-time position calculation at boid birth (~6 multiplications per thruster)
3. A visualisation update to draw thrusters at their evolved positions

The payoff is that we don't have to commit to a single thruster layout upfront. We can run experiments with N=2, N=3, N=4, with or without constraints, and let evolution show us what works. The fixed 4-thruster layout from Model 1 is just one possible genome configuration within Model 2 — we could even seed initial populations with that layout and let evolution depart from it.

The practical approach: **implement Model 2 from the start** (it's barely harder), but **seed initial populations with a sensible symmetric layout** (e.g. the Model 1 arrangement) so evolution has a working starting point rather than random thruster spaghetti. Mutation will then explore variations from that baseline.

### Mutable vs Locked Thruster Geometry

Crucially, Model 2 should support a **per-gene mutability flag** on thruster position/angle genes. This gives us three operational modes from the same codebase:

| Mode | Thruster count | Positions/angles | Use case |
|------|---------------|-------------------|----------|
| **Fully locked** | Fixed N | Pre-defined, immutable | Equivalent to Model 1. Test a specific layout (e.g. the 4-thruster design, or a 2-thruster differential) without any body-plan evolution. Useful for isolating brain evolution from body evolution. |
| **Fully mutable** | Fixed N | Inherited + mutated | Full body-plan evolution. Thruster positions drift over generations. Most exploratory mode. |
| **Partially mutable** | Fixed N | Some locked, some free | E.g. lock one rear thruster in place for guaranteed forward propulsion, let the others evolve freely. Hybrid approach to prevent total degeneracy while allowing creativity. |

Implementation-wise, this is just a boolean per thruster-position gene: `mutable: true/false`. When `false`, crossover and mutation skip that gene — it's inherited as-is from the initial seed. The physics loop doesn't know or care; it reads **r**_i and **d**_i either way.

This means we can run controlled experiments: start with locked geometry to get brain evolution working and tuned, then flip positions to mutable and observe what body-plan changes evolution discovers. Or compare: does a locked symmetric 3-thruster layout produce better predators than a mutable one where evolution finds its own arrangement?

---

## Sources

- [Craig Reynolds' Steering Behaviors (1999)](https://www.red3d.com/cwr/steer/gdc99/)
- [Tu & Terzopoulos, Artificial Fishes: Physics, Locomotion, Perception, Behavior (SIGGRAPH '94)](https://dl.acm.org/doi/10.1145/192161.192170)
- [2D Spaceship Thruster Physics (Envato Tuts+)](https://code.tutsplus.com/using-torque-and-thrusters-to-move-and-rotate-a-player-designed-spaceship--gamedev-2610t)
- [Video Game Physics Tutorial: Rigid Body Dynamics (Toptal)](https://www.toptal.com/game/video-game-physics-part-i-an-introduction-to-rigid-body-dynamics)
- [MIT Lecture: 2D Rigid Body Dynamics](https://ocw.mit.edu/courses/16-07-dynamics-fall-2009/befffaf20475c1a379c3ac52e91a78cb_MIT16_07F09_Lec21.pdf)
- [Energy-Based Models for Virtual Creatures (ALIFE 2024)](https://direct.mit.edu/isal/proceedings/isal2024/36/30/123447)
- [Monte Carlo Analysis of Boid Simulations (2024)](https://arxiv.org/html/2412.10420v1)
- [ALIEN: Artificial Life Environment](https://www.alien-project.org/index.html)
- [Nature of Code: Autonomous Agents](https://natureofcode.com/autonomous-agents/)
- [Deep RL for Vectored Thruster AUV Control](https://onlinelibrary.wiley.com/doi/10.1155/2021/6649625)
- [Asteroids-style Physics in Godot](https://kidscancode.org/godot_recipes/4.x/physics/asteroids_physics/index.html)
