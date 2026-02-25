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
