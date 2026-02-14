# Physically-Grounded Boid Locomotion

CLaude-generated doc. Prompt: "OK, in the new markdown script @boid_theory.md we're going to explore some new options that might fit better with exploring evolution impacts. First, digest again everything in @evolution_theory.md. Then - in my 2007 wildboids, an issue I had was that boids evolved to "hack" the underlying physics (which weren't really physics, as Reynolds' verison isn't , it's psuedo-physics). So I am thinking of making the boids more physically grounded. My current idea: use actual thrust vectors and thrust for rotation (which will also allow energy to be used and run out). I think (but feel free to make other suggestions) a simple option would be one thruster each on front and back, and then two side thrusters near the back that allow for rotation. Find some sources and lay out some thoughts on this more physically grounded approach."

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

## Proposed Model: Thruster-Based 2D Rigid Body

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
