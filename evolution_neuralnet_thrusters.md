# Evolved Neural Control: Sensors to Thrusters

Claude-generated doc. Based on the thrust-based physics model in [boid_theory.md](boid_theory.md) and the evolution framework in [evolution_theory.md](evolution_theory.md).

Claude thread [here](llm_convos/2026-02-15_1740_The_user_opened_the_file_UsersdanolnerCodeclaudewi.md).

---

**Purpose:** Define a modular, evolvable framework that connects arbitrary sensory inputs to thruster commands. Rather than hard-wiring "sensor X drives thruster Y," we want an intermediate processing layer — something neural-net-like — where the connection structure, weights, and even the *learning rules themselves* can evolve.

---

## Design Principles

### Loose Coupling, Clear Boundaries

The system has three cleanly separated layers. Each can be developed, tested, and modified independently.

```
┌─────────────────────────────────────────────────────────────────┐
│                        SENSORY LAYER                            │
│  Reads world state → produces normalised input signals          │
│  (What the boid perceives)                                      │
└───────────────────────────┬─────────────────────────────────────┘
                            │  float inputs[N_SENSORS]
                            │  all values in [0, 1] or [-1, 1]
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                     PROCESSING NETWORK                          │
│  Evolved topology, weights, and plasticity rules                │
│  (How the boid thinks)                                          │
└───────────────────────────┬─────────────────────────────────────┘
                            │  float outputs[N_THRUSTERS]
                            │  all values in [0, 1]
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                      THRUSTER LAYER                             │
│  Converts commands to forces/torques on rigid body              │
│  (How the boid moves — see boid_theory.md)                      │
└─────────────────────────────────────────────────────────────────┘
```

**The interfaces are simple arrays of floats.** The sensory layer doesn't know about thrusters. The thruster layer doesn't know about sensors. The processing network only knows it receives N inputs and must produce M outputs. This means:

- We can swap sensory systems without touching the network or physics
- We can change thruster count/layout without touching sensors or network topology
- We can experiment with different network architectures while keeping sensors and thrusters identical

### Object-Oriented Structure (C++)

```cpp
// The three layers as abstract interfaces

class SensorySystem {
public:
    virtual ~SensorySystem() = default;
    virtual int inputCount() const = 0;
    virtual void perceive(const World& world, const Boid& self,
                          float* outputs) const = 0;
};

class ProcessingNetwork {
public:
    virtual ~ProcessingNetwork() = default;
    virtual void activate(const float* inputs, int numInputs,
                          float* outputs, int numOutputs) = 0;
    virtual void reset() = 0;  // clear any internal state between episodes
};

class ThrusterArray {
public:
    virtual ~ThrusterArray() = default;
    virtual int thrusterCount() const = 0;
    virtual void applyThrust(const float* commands, RigidBody& body,
                             float dt, float& energyCost) const = 0;
};

// A Boid composes all three
class Boid {
    RigidBody body;
    std::unique_ptr<SensorySystem> sensors;
    std::unique_ptr<ProcessingNetwork> brain;
    std::unique_ptr<ThrusterArray> thrusters;
    Genome genome;
    float energy;

public:
    void update(const World& world, float dt) {
        // 1. Sense
        std::vector<float> inputs(sensors->inputCount());
        sensors->perceive(world, *this, inputs.data());

        // 2. Think
        std::vector<float> commands(thrusters->thrusterCount());
        brain->activate(inputs.data(), inputs.size(),
                        commands.data(), commands.size());

        // 3. Act
        float cost = 0;
        thrusters->applyThrust(commands.data(), body, dt, cost);
        energy -= cost;

        // 4. Physics integration (drag, velocity, position)
        body.integrate(dt);
    }
};
```

This is the skeleton. Each layer has its own genome segment, its own mutation operators, and its own evolutionary story — but they co-evolve within the same organism.

---

## Layer 1: Sensory Inputs

Sensors read from the world and produce normalised floats. We can define *any* sensor we want — the processing network doesn't care what the numbers mean, only that they arrive.

### Sensor Design Philosophy

Sensors should be **information-rich but computationally cheap**. Each sensor is a function: `World × Boid → float`. We can mix and match freely.

### Candidate Sensor Types

| Sensor | Output | What It Captures | Cost |
|--------|--------|------------------|------|
| **Sector density** | 0–1 | Count of entities in a cone (angle, radius) normalised by max | O(nearby) via spatial grid |
| **Nearest distance** | 0–1 | Distance to closest entity in a cone, normalised by max range | O(nearby) |
| **Nearest angle** | -1 to 1 | Relative bearing to closest entity (left/right of heading) | O(nearby) |
| **Closing speed** | -1 to 1 | Rate of approach/recession of nearest entity | O(nearby) |
| **Own speed** | 0–1 | Current linear velocity normalised by terminal velocity | O(1) |
| **Own angular velocity** | -1 to 1 | Current rotation rate | O(1) |
| **Energy level** | 0–1 | Current energy fraction | O(1) |
| **Wall proximity** | 0–1 | Distance to nearest world boundary (if not toroidal) | O(1) |
| **Food direction** | -1 to 1 | Bearing to nearest food source | O(nearby) |
| **Predator alarm** | 0–1 | Is any predator within danger radius? | O(nearby) |

### Evolvable Sensor Parameters

Each sector-based sensor can have **evolved parameters** controlling where and how far it looks:

```cpp
struct SectorSensor {
    float centerAngle;   // -π to π relative to heading
    float arcWidth;      // 0 to 2π (how wide the cone is)
    float maxRange;      // detection radius
    EntityFilter filter;  // what to detect: own-kind, other-kind, food, all
    SignalType signal;    // what to report: count, nearest_dist, nearest_angle, closing_speed
};
```

A boid with 5 sector sensors could evolve:
- A narrow forward sensor detecting predators at long range (early warning)
- A wide rear sensor detecting anything nearby (ambush detection)
- Two side sensors for flock alignment
- A short-range omnidirectional food detector

The sensor parameters are part of the genome. Evolution shapes *what the boid pays attention to*, not just how it responds.

### Sensor Count

Fixed per species (predator vs prey may differ), but a design parameter we choose. Start with **5–8 sensors** per boid — enough for interesting behavior, small enough for tractable evolution.

---

## Layer 2: The Processing Network

This is the core design challenge. We need a framework that:

1. Takes N sensor inputs and produces M thruster outputs
2. Has evolvable parameters (weights, biases, topology)
3. Supports modularity — not just "every input connects to every output"
4. Can potentially learn/adapt during a boid's lifetime (plasticity)
5. Is computationally cheap enough to run per-boid per-frame

### Why Not Direct Mapping?

The simplest approach from [evolution_theory.md](evolution_theory.md) was a direct weight matrix: each sensor has a weight per thruster, and the thruster command is a weighted sum of sensor activations.

```
thruster_j = sigmoid( Σ_i  sensor_i × weight_ij + bias_j )
```

This works but is limiting:

- **No nonlinear combination of inputs.** A boid can't learn "flee only when predator is close AND I'm low on energy" — that requires combining two inputs nonlinearly before they reach the thrusters.
- **No internal state.** Every frame is independent. A boid can't build up an "alarm level" over time or remember that a predator was recently nearby.
- **No modularity.** Every sensor connects to every thruster with equal structural opportunity. There's no way for evolution to create specialised sub-circuits.

We need at least one intermediate layer — and ideally a flexible one.

---

## Network Architecture Options

### Option A: Fixed Topology, Evolved Weights (Simplest)

A hand-designed feedforward network with one hidden layer. Evolution only adjusts the weights and biases.

```
Sensors (5-8)  →  Hidden (4-8 nodes)  →  Thrusters (3-4)
            fully connected        fully connected
```

**Genome:** A flat vector of floats — all connection weights plus biases. For 6 sensors, 6 hidden nodes, 4 thrusters: `(6×6) + 6 + (6×4) + 4 = 70 genes`.

**Pros:**
- Simplest to implement
- Fixed genome size — trivial crossover
- Well-understood dynamics

**Cons:**
- Topology choice is arbitrary and may be suboptimal
- No structural adaptation — can't simplify or complexify as needed
- [Research shows topology evolution alone is more effective than weight evolution alone](https://link.springer.com/article/10.1023/A:1008272615525)

**Verdict:** Good starting point for debugging, but we'd want to move beyond it.

---

### Option B: NEAT — Evolving Topology and Weights

[NEAT (NeuroEvolution of Augmenting Topologies)](https://nn.cs.utexas.edu/downloads/papers/stanley.ec02.pdf) is the gold standard for evolving both network structure and weights simultaneously. Developed by Stanley & Miikkulainen (2002).

**How it works:**

1. **Start minimal.** All networks begin as direct input→output connections with no hidden nodes. This is the simplest possible topology.

2. **Structural mutations** add complexity over time:
   - *Add connection:* new link between two previously unconnected nodes
   - *Add node:* split an existing connection, inserting a node in between

   > **When do structural mutations happen? And can networks shrink?**
   >
   > Structural mutations are **purely random**, with fixed probabilities per genome per generation. In the original paper: ~3% chance of adding a node, ~5% chance of adding a connection (higher in larger populations). There's no fitness-based trigger — the mutation fires or it doesn't, and selection determines whether the result survives.
   >
   > **In original NEAT, networks can only grow, not shrink.** There is no "delete node" or "delete connection" mutation. Connections can be *disabled* (via a toggle mutation at ~1% rate), and disabled connections have a 25% chance of being re-enabled during crossover. But a disabled connection still exists in the genome — it's dormant, not removed. The genome is append-only.
   >
   > This raises an obvious concern: **won't networks bloat indefinitely?** In original NEAT, the answer is "somewhat, yes." The intended safeguards are indirect:
   >
   > - **Starting minimal** means there's nothing to bloat initially — complexity must earn its place
   > - **Speciation + fitness sharing** means bloated offspring that drift into a new species must outperform their leaner ancestors to survive
   > - **Selection pressure** culls networks where added structure doesn't improve fitness
   >
   > But these are soft constraints. Research has shown that when NEAT hits a fitness plateau, the downward pressure on complexity weakens — there's no fitness *improvement* to select for, so structural mutations accumulate without being selected against. One study found original NEAT [continuously growing to ~13 hidden nodes with ~45 added connections](https://www.cs.swarthmore.edu/~meeden/cs81/s12/papers/DasolEmilyPaper.pdf) on benchmarks where simpler solutions exist.
   >
   > **Do networks "settle down"?** Not automatically. Structural mutations keep firing at their fixed probabilities forever. In practice, networks reach phases where most structural mutations are neutral or harmful and get selected against — but the mutations don't *stop*; they just mostly fail to propagate. If the population reaches a fitness plateau, complexity can drift upward unchecked.
   >
   > **Modern NEAT variants fix this.** [SharpNEAT](https://sharpneat.sourceforge.io/phasedsearch.html) (Colin Green) introduced **phased searching**: the algorithm alternates between complexification mode (normal NEAT) and pruning mode, where additive mutations are disabled and deletion mutations are enabled. Connections are randomly removed; if this strands a neuron (no remaining inputs or outputs), the neuron is deleted too. The result is an oscillating complexity profile — grow, prune, grow, prune — that explores complex topologies without permanent bloat. [NEAT-Python](https://neat-python.readthedocs.io/en/latest/neat_overview.html) similarly adds configurable `node_delete_prob` and `conn_delete_prob` parameters.
   >
   > **For Wild Boids, we should implement pruning from the start** — either SharpNEAT-style phased searching or simple deletion mutations alongside additions. Our boid brains should be small (10–25 nodes), and pruning prevents them from accumulating structural dead weight over long evolutionary runs.

3. **Historical markings.** Every new gene (connection) gets a globally unique *innovation number*. This solves the alignment problem during crossover — genes with matching innovation numbers correspond to the same structural element and can be crossed over meaningfully.

4. **Speciation.** Networks are grouped into species by structural similarity. Individuals compete primarily within their species, protecting new innovations from being immediately outcompeted by established topologies.

   > **Why do innovations need "protection"? Isn't surviving competition the whole point of evolution?**
   >
   > Yes — but there's a timing problem specific to *topology* mutations (as opposed to weight mutations). When NEAT adds a new hidden node by splitting an existing connection, the new node starts with essentially random or neutral weights. It hasn't had time to *optimise* those weights yet. In the generation it appears, it almost certainly performs *worse* than the established topology it's competing against, because the existing simpler networks have had many generations to tune their weights.
   >
   > This is the **problem of conventions** (also called the **competing conventions problem**). A structural innovation that *would* be beneficial after 10–20 generations of weight tuning gets killed in generation 1 because it's competing against already-tuned networks. Without speciation, NEAT degenerates into fixed-topology weight evolution — structural mutations appear and are immediately eliminated, so complexity never increases.
   >
   > The biological analogy: imagine a mutation that gives an animal a new organ (say, a proto-eye). The proto-eye is initially useless — it's metabolically expensive and provides no survival advantage until supporting neural circuitry co-evolves to process its signals. In a population where every individual competes directly against every other, the proto-eye carrier is just a less-efficient version of its eyeless competitors. But if proto-eye carriers are somewhat isolated (geographically, ecologically), they get time to refine the organ before competing with the wider population. This is essentially what speciation does in NEAT — it's analogous to [allopatric speciation](https://en.wikipedia.org/wiki/Allopatric_speciation) in biology, where geographic isolation allows divergent adaptations to develop.
   >
   > Stanley & Miikkulainen tested this directly: NEAT without speciation solved their benchmark (double pole balancing) only [20% of the time and took 8.5× more evaluations when it did succeed](https://nn.cs.utexas.edu/downloads/papers/stanley.ec02.pdf). The original paper explicitly identifies this as one of NEAT's three key contributions, arguing that "weights of a new structure are unlikely to be optimized right away" and that speciation provides the "breathing room" needed. [Lehman et al. (2008)](https://eplex.cs.ucf.edu/papers/lehman_gecco08.pdf) further confirmed that speciation is critical for maintaining population diversity and preventing premature convergence to local optima.
   >
   > It's worth noting: this is less of a problem for *weight-only* evolution (like Option A above), where every genome has the same topology and mutations are incremental adjustments to existing parameters. It's specifically the *structural* mutations — adding nodes and connections — that need this grace period. A weight perturbation either helps or it doesn't, immediately. A new hidden node needs time to find its role.

5. **Complexification.** Networks only grow when complexity helps. If a simple direct sensor→thruster mapping is sufficient, NEAT will keep it simple. Hidden nodes and recurrent connections emerge only when they improve fitness.

```
Generation 0:          Generation 50:            Generation 200:

S1 ──→ T1             S1 ─┬──→ H1 ──→ T1       S1 ──→ H1 ─┬──→ T1
S2 ──→ T2             S2 ─┤    │       │        S2 ──→ H2 ─┤    │
S3 ──→ T3             S3 ─┘    └──→ T2 │        S3 ──→ H3 ─┘    │
                                    T3←─┘        S4 ──→ H1        │
                                                 H2 ──→ H3 (recurrent)
                                                 H3 ──→ T2
                                                        T3←── H2
(direct mapping)       (one hidden node          (multiple hidden,
                        with fan-out)              recurrent connection)
```

**For our use case (5-8 inputs, 3-4 outputs):**

NEAT is well-suited here. The networks stay small, so the overhead of speciation and innovation tracking is negligible. Population sizes of 150–200 are typically sufficient. And NEAT's biggest advantage is precisely what we want: it discovers the *right level of complexity* rather than forcing us to guess.

**C++ implementations exist:** [EvolutionNet](https://github.com/BiagioFesta/EvolutionNet), [tinyai](https://github.com/hav4ik/tinyai), or Kenneth Stanley's [original C++ code](https://github.com/FernandoTorres/NEAT/).

**Pros:**
- Automatic topology discovery — no need to guess hidden layer size
- Starts simple, complexifies only when beneficial
- Speciation protects innovation
- Can discover recurrent connections (memory/internal state)
- Well-proven on agent control tasks

**Cons:**
- More complex to implement than fixed topology
- Variable-size genomes require NEAT-specific crossover
- Speciation parameters need tuning (compatibility threshold, species target)

---

### Build vs Buy: Bespoke NEAT or Library?

The NEAT algorithm is well-specified but has many moving parts (innovation tracking, speciation, compatibility distance, crossover with disjoint/excess genes, structural mutations, topological sorting for activation). Before committing, we need to decide: write it ourselves, or use an existing C++ library?

#### What's Out There

| Library | License | Last Active | Quality | Plasticity Support |
|---------|---------|-------------|---------|-------------------|
| [MultiNEAT](https://github.com/peter-ch/MultiNEAT) | LGPL-3.0 | ~2018 | Best available — 333 stars, 463 commits, clean API, supports HyperNEAT and recurrent networks | Partial: has `Adapt()` with 2-parameter Hebbian rule, but not the full ABCD model |
| [FernandoTorres/NEAT](https://github.com/FernandoTorres/NEAT/) | Apache 2.0 | ~2013 | Stanley's reference implementation, reorganised. Algorithmically authoritative but pre-C++11 code style. Not designed as an embeddable library | The original "trait" system carries per-gene parameters, but no Hebbian rule |
| [EvolutionNet](https://github.com/BiagioFesta/EvolutionNet) | GPL-3.0 | ~2021 | Header-only C++17, but only 6 commits, no documentation, no usage examples. Hobbyist project | None |
| [tinyai](https://github.com/hav4ik/tinyai) | Unknown | Unknown | 83% HTML, 16% C++ — more demo/blog than library | None |
| [PNEATM](https://github.com/abadiet/PNEATM) | MIT | Oct 2024 | Actively developed, v1.0.0. Has temporal memory extensions. But one-person project with known convergence issues | None |

**Bottom line:** MultiNEAT is the only library with a real API, real users, and real correctness. But even MultiNEAT stalled around 2018, requires LGPL compliance, and — critically — doesn't support the full ABCD Hebbian plasticity model we want for Phase 3+. Every library would need the same plasticity extensions bolted on.

#### The Case for Bespoke

For our specific use case (5–8 inputs, 3–4 outputs, populations of 150–200, and a custom plasticity model), writing our own NEAT has genuine advantages:

**1. The algorithm is well-documented and the networks are tiny.**

NEAT is not a black box. The [original paper](https://nn.cs.utexas.edu/downloads/papers/stanley.ec02.pdf) gives complete pseudocode, and the core data structures are straightforward:

```cpp
struct NodeGene { int id; NodeType type; ActivationFn activation; float bias; };
struct ConnectionGene {
    int innovation;
    int source, target;
    float weight;
    bool enabled;
    // Plasticity (Phase 3+):
    float alpha, eta, A, B, C, D;
    float plastic_weight;  // runtime, not inherited
};
struct Genome { std::vector<NodeGene> nodes; std::vector<ConnectionGene> connections; };
```

That's it. The genome is two vectors. For networks in the 10–25 node range with 20–50 connections, everything fits in cache. There is no deep framework to wrangle — it's data structures plus a handful of operations (mutate, crossover, speciate, activate).

**2. Plasticity is first-class, not bolted on.**

Our recommended architecture (Option C below) adds 6 parameters per connection gene (α, η, A, B, C, D). In a bespoke implementation, these live directly in `ConnectionGene` and flow naturally through mutation, crossover, and activation. In a library, they'd have to be shoehorned into the library's gene representation — MultiNEAT's `m_Traits` map, or Stanley's trait system — and the `Adapt()` / activation loop would need patching. At that point you're maintaining a fork, which is worse than owning the code outright.

**3. We control the interface.**

Our architecture has a clean contract: `float sensors[N] → network → float thrusters[M]`. A bespoke implementation can expose exactly this:

```cpp
class NeatNetwork {
public:
    void activate(const float* inputs, int n_in, float* outputs, int n_out);
    void update_plasticity(float dt);
    void reset();  // clear plastic weights between episodes
};
```

No impedance mismatch with a library's assumptions about how populations are managed, how fitness is assigned, or how experiments are structured. The `ProcessingNetwork` interface from the architecture diagram above maps 1:1 to a class we write.

**4. The total code is small.**

A complete NEAT implementation for our scale is roughly:

| Component | Estimated Lines | Notes |
|-----------|----------------|-------|
| Genome (nodes + connections) | ~100 | Two structs, serialisation |
| Mutation operators | ~150 | Weight perturb, add node, add connection, toggle, plasticity params |
| Crossover | ~80 | Innovation-number alignment, disjoint/excess handling |
| Speciation | ~100 | Compatibility distance, species assignment, fitness sharing |
| Network activation | ~80 | Topological sort, forward pass, sigmoid outputs |
| Plasticity update | ~40 | ABCD rule per connection |
| Population management | ~120 | Selection, reproduction, elitism, species culling |
| **Total** | **~670** | Spread across 3–4 files |

For context, `sensory_system.cpp` is ~70 lines and `spatial_grid.cpp` is ~60 lines. The NEAT implementation is larger but not qualitatively different from what we've already built. And every line is ours to understand, debug, and extend.

**5. No dependency risk.**

MultiNEAT is LGPL (linking constraints), stale since 2018, and has no versioned releases. The other libraries are dead or immature. A bespoke implementation has zero external risk and zero licensing concerns.

#### What About GPU?

Short answer: **irrelevant at our scale.** The networks are too small and the population too small for GPU to help.

The only GPU-accelerated NEAT work is [TensorNEAT](https://arxiv.org/abs/2404.01817) (JAX/Python), which achieves 500× speedups — but on RTX 4090 with populations of 1,000–10,000 and networks with hundreds of nodes. Their approach tensorizes diverse NEAT topologies into uniform padded tensors (padded to the max network size in the population), then batch-evaluates the entire population in parallel.

For our parameters:
- **Network forward pass:** ~50 multiply-adds for a 25-node network. On a single CPU core, that's nanoseconds.
- **Population evaluation:** 200 networks × 50 ops = 10,000 multiply-adds per generation step. Still microseconds.
- **GPU overhead:** Kernel launch + memory transfer latency alone (~5–50μs) exceeds the entire CPU computation.
- **Topology diversity:** NEAT populations have heterogeneous topologies. GPU execution requires padding every network to the largest topology in the population, wasting most of the allocated tensor space. At 25 max nodes this is barely a 25×25 matrix — GPU parallelism needs thousands of elements to amortize overhead.
- **The real bottleneck is simulation, not network activation.** Each fitness evaluation runs the boid through many simulation timesteps (physics, spatial grid, sensor queries). This is the expensive part, and it's inherently sequential per-boid. GPU doesn't help unless you parallelise the entire simulation, which is a fundamentally different project.

> **Rule of thumb:** GPU NEAT becomes worthwhile when `population_size × network_size > ~100,000`. For our 200 × 25 = 5,000, we're two orders of magnitude below that threshold. CPU is not just adequate — it's optimal. No memory transfer overhead, no kernel launch latency, no tensor padding waste, full access to the simulation state without copying.

If the population ever scales to thousands (e.g. island-model evolution), CPU parallelism via `std::thread` or OpenMP across subpopulations would be the right first step, not GPU.

#### How a Library Would Plug In (If We Used One)

For concreteness — if we chose MultiNEAT, the sensor-to-thruster pipeline would look like this:

```cpp
#include <MultiNEAT/MultiNEAT.h>

// === Setup (once, at population initialisation) ===

NEAT::Parameters params;
params.PopulationSize = 150;
params.MutateWeightsProb = 0.8;
params.MutateAddNeuronProb = 0.02;
params.MutateAddLinkProb = 0.05;
// ... ~30 more parameters to configure

int n_sensors = 7;   // from boid spec
int n_thrusters = 4; // from boid spec

// Seed genome: n_sensors inputs + 1 bias, no hidden nodes, n_thrusters outputs
NEAT::Genome seed(0, n_sensors + 1, 0, n_thrusters, false,
                  NEAT::UNSIGNED_SIGMOID, NEAT::UNSIGNED_SIGMOID,
                  0, params, 0);

NEAT::Population pop(seed, params, true, 1.0, random_seed);

// === Per-generation evaluation ===

for (auto& species : pop.m_Species) {
    for (auto& individual : species.m_Individuals) {

        // Build the phenotype (runtime network) from genotype
        NEAT::NeuralNetwork net;
        individual.BuildPhenotype(net);

        // --- This is where our architecture connects ---

        // 1. SENSE: run our SensorySystem (unchanged)
        float sensor_outputs[7];
        boid.sensors->perceive(boids, grid, config, self_idx, sensor_outputs);

        // 2. THINK: feed sensors into the NEAT network
        std::vector<double> inputs(n_sensors + 1);  // MultiNEAT uses double
        for (int i = 0; i < n_sensors; i++)
            inputs[i] = static_cast<double>(sensor_outputs[i]);
        inputs[n_sensors] = 1.0;  // bias input (MultiNEAT convention)

        net.Input(inputs);
        net.Activate();  // forward pass

        std::vector<double> outputs = net.Output();

        // 3. ACT: map network outputs to thruster power
        for (int i = 0; i < n_thrusters; i++)
            boid.thrusters[i].power = static_cast<float>(outputs[i]);
        // outputs are already [0,1] — sigmoid output nodes

        // --- End of pipeline ---

        // Run simulation, accumulate fitness...
        individual.SetFitness(fitness);
    }
}

pop.Epoch();  // advance generation: selection, crossover, mutation, speciation
```

**The coupling points are exactly two:**
1. **Sensor outputs → `net.Input()`**: copy our `float[]` into MultiNEAT's `std::vector<double>` (note the float→double conversion — MultiNEAT uses double internally)
2. **`net.Output()` → thruster power**: copy MultiNEAT's `std::vector<double>` back to our `float` thruster powers

Everything else (our sensors, our physics, our world) is untouched. The library is a black box between `Input()` and `Output()`.

**The awkwardness:** MultiNEAT owns the population loop, the genome representation, and the mutation operators. Adding ABCD plasticity means:
- Extending `LinkGene` with 6 new fields (or abusing the `m_Traits` map)
- Adding mutation operators for those fields (patching `Genome::Mutate()`)
- Extending `Genome::Mate()` to carry plasticity params through crossover
- Replacing `NeuralNetwork::Adapt()` with our ABCD rule
- Extending the compatibility distance to include plasticity parameter differences

At that point, you're maintaining a ~4,000-line fork of a stale LGPL library. The bespoke ~670-line implementation starts looking much more attractive.

#### Verdict

**Write it ourselves.** The algorithm is well-specified, the networks are tiny, plasticity needs to be first-class, GPU is irrelevant at our scale, and every available library either lacks what we need or would require forking. The total code is comparable to what we've already written for the spatial grid and sensory system. We keep full control, zero dependencies, and a clean interface that matches our architecture exactly.

---

### Option C: NEAT + Evolved Plasticity (Our Recommendation)

This is where it gets interesting — and where the user's intuition about "evolutionarily flexible plasticity per connection" maps onto cutting-edge research.

#### The Core Idea

In standard neural networks (including NEAT), connection weights are fixed for a boid's lifetime — set at birth from the genome, never changing. But biological synapses aren't like this. They strengthen or weaken based on experience: [Hebbian learning](https://en.wikipedia.org/wiki/Hebbian_theory) ("neurons that fire together, wire together").

What if each connection in the boid's brain had **two components**:

1. **A fixed weight** (w) — set by the genome, inherited from parents, subject to mutation
2. **A plastic component** (p) — starts at zero, changes during the boid's lifetime based on local neural activity

The effective weight at any moment is:

```
effective_weight = w + α × p
```

Where **α** (alpha) is an **evolved plasticity coefficient** — also part of the genome, also per-connection. This is the key: **evolution doesn't just decide the wiring and the weights, it decides *how much each connection can learn* during the boid's life.**

#### The Plasticity Rule

The plastic component p updates each timestep according to a Hebbian rule:

```
Δp = η × (A × pre × post + B × pre + C × post + D)
```

Where:
- `pre` = activation of the presynaptic (source) neuron
- `post` = activation of the postsynaptic (target) neuron
- `η` = learning rate (evolved, per-connection or global)
- **A, B, C, D** = evolved coefficients that define the learning rule

This is the **ABCD model** from [meta-learning through Hebbian plasticity](https://proceedings.neurips.cc/paper/2020/file/ee23e7ad9b473ad072d57aaa9b2a5222-Paper.pdf) research. Different values of A, B, C, D produce different learning dynamics:

| A | B | C | D | Behavior |
|---|---|---|---|----------|
| 1 | 0 | 0 | 0 | Pure Hebbian: connections strengthen when both neurons are active |
| -1 | 0 | 1 | 0 | Anti-Hebbian with postsynaptic bias: weakens correlated activity |
| 1 | 0 | 0 | -0.1 | Hebbian with decay: learns associations but slowly forgets |
| 0 | 0 | 0 | 0 | No plasticity (connection is purely genetic, like standard NEAT) |

The crucial point: **evolution doesn't hand-design the learning rule.** It evolves A, B, C, D, η, and α as genes. Some connections might evolve to be highly plastic (large α, active learning rule), others might evolve to be purely fixed (α ≈ 0). The *pattern of plasticity across the network* is itself an evolved trait.

#### What This Buys Us

**Lifetime adaptation.** A boid born into a world with aggressive predators can *learn* evasion patterns during its life, not just rely on innate reflexes. A predator can adapt its pursuit strategy based on what works against the current prey population.

**Faster evolution.** [Miconi et al. (2018)](https://arxiv.org/abs/1804.02464) showed that networks with evolved plasticity solve tasks that non-plastic networks cannot, and converge faster on tasks both can solve. The plasticity provides a form of within-lifetime "fine-tuning" that reduces the precision required from genetic encoding.

**Memory and habituation.** Plastic connections can implement simple memory: a predator encounter strengthens "alarm" connections for some time after, producing lingering vigilance. This is impossible with fixed weights.

**Different boids, different learning styles.** One prey species might evolve connections with fast, volatile plasticity (quick to panic, quick to calm). Another might evolve slow, stable plasticity (gradually learns predator patterns over its lifetime). These are different *evolved strategies for learning*, not different behaviors directly — a meta-level that's genuinely novel compared to most boid simulations.

#### Per-Connection Genome

With NEAT + plasticity, each connection gene contains:

```cpp
struct ConnectionGene {
    int innovationNumber;  // NEAT historical marking
    int sourceNode;
    int targetNode;

    // Standard NEAT
    float weight;          // fixed weight (w)
    bool enabled;

    // Plasticity parameters (all evolved)
    float alpha;           // plasticity coefficient: how much plastic
                           //   component contributes (0 = purely genetic)
    float eta;             // learning rate
    float A, B, C, D;     // Hebbian rule coefficients

    // Runtime state (not inherited)
    float plasticWeight;   // current plastic component (p), starts at 0
};
```

That's 6 extra floats per connection gene compared to standard NEAT. For a small network with ~20 connections, that's 120 extra floats — trivial.

#### Connection Update During Lifetime

```cpp
void Connection::update(float preActivation, float postActivation, float dt) {
    // Hebbian plasticity update
    float delta = eta * (A * preActivation * postActivation
                       + B * preActivation
                       + C * postActivation
                       + D);

    plasticWeight += delta * dt;

    // Optional: clamp to prevent runaway
    plasticWeight = std::clamp(plasticWeight, -1.0f, 1.0f);
}

float Connection::effectiveWeight() const {
    return weight + alpha * plasticWeight;
}
```

---

## Layer 2.5: Neuromodulation (Optional Extension)

This is the most speculative part, but has strong theoretical grounding and maps well to the user's interest in "overlapping connections."

### The Idea

In biological brains, not all neurons directly contribute to motor output. Some neurons produce **modulatory signals** — like dopamine or serotonin — that don't drive muscles but instead *regulate how other neurons learn and respond*. A dopamine surge after eating doesn't make you move; it makes the neural pathways that led to finding food *stronger*.

In our framework, we could add a **modulatory neuron type** alongside standard neurons:

```
Standard neuron:    receives inputs → activation → sends to targets
Modulatory neuron:  receives inputs → activation → modulates plasticity of target connections
```

A modulatory neuron's output doesn't feed into the weighted sum of its targets. Instead, it **scales the learning rate (η)** of connections in its target region:

```cpp
// For a connection modulated by modulatory neuron m:
float modulatedEta = eta * modulatoryOutput;  // 0 = no learning, 1 = full learning rate

float delta = modulatedEta * (A * pre * post + B * pre + C * post + D);
plasticWeight += delta * dt;
```

### What This Enables

- **Context-dependent learning.** A boid only updates its "predator response" connections when a modulatory "danger" neuron is active. In safe periods, those connections are stable.
- **Reward-gated plasticity.** A modulatory neuron could fire when the boid successfully eats, strengthening whatever behavioral pathway was active at that moment. This is a crude form of reinforcement learning, but *entirely evolved* — no external reward signal is needed; the boid's own neural circuit determines what counts as "reward."
- **Selective memory.** Different brain regions can have different plasticity regimes, controlled by different modulatory neurons.

### Relationship to NEAT

In NEAT, modulatory neurons would simply be a node type. NEAT can evolve when and where to insert them, just like it evolves hidden nodes. The innovation system handles it automatically — a mutation adds a modulatory node, speciation protects it while it's tuned.

[Soltoggio et al.](https://andrea.soltoggio.net/data/papers/SoltoggioALife2008.pdf) demonstrated that when NEAT is extended with modulatory neurons, evolution *spontaneously discovers* useful modulatory circuits in tasks with changing reward conditions. The modulatory neurons aren't hand-placed — evolution inserts them where they help.

### Implementation Cost

Per-frame, each modulatory neuron is just one extra activation calculation. The modulation itself is a single multiply on the learning rate. For networks with 2–3 modulatory neurons among 10–20 total nodes, the cost is negligible.

---

## Putting It Together: The Full Architecture

### Genome Structure

```
┌────────────────────────────────────────────────────────────────┐
│                         BOID GENOME                            │
│                                                                │
│  ┌──────────────────┐  ┌──────────────────┐  ┌─────────────┐  │
│  │  SENSOR GENES    │  │  NETWORK GENES   │  │ THRUSTER     │  │
│  │                  │  │  (NEAT format)   │  │ GENES        │  │
│  │  Per sensor:     │  │                  │  │              │  │
│  │  - center angle  │  │  Node genes:     │  │ Per thruster:│  │
│  │  - arc width     │  │  - type (std/mod)│  │ - position x │  │
│  │  - max range     │  │  - bias          │  │ - position y │  │
│  │  - entity filter │  │  - activation fn │  │ - fire angle │  │
│  │  - signal type   │  │                  │  │ - max thrust │  │
│  │                  │  │  Connection genes:│  │ - mutability │  │
│  │  (fixed count,   │  │  - innovation #  │  │   flag       │  │
│  │   evolved params)│  │  - source/target │  │              │  │
│  │                  │  │  - weight (w)    │  │ (Model 2 from│  │
│  │                  │  │  - enabled       │  │  boid_theory) │  │
│  │                  │  │  - alpha (α)     │  │              │  │
│  │                  │  │  - eta (η)       │  │              │  │
│  │                  │  │  - A, B, C, D    │  │              │  │
│  └──────────────────┘  └──────────────────┘  └─────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

### Per-Frame Execution

```
1. SENSE      sensors->perceive(world, self, sensorBuffer)       O(nearby × sensors)
2. ACTIVATE   brain->activate(sensorBuffer, thrusterBuffer)      O(connections)
3. LEARN      brain->updatePlasticity(dt)                        O(connections)
4. THRUST     thrusters->apply(thrusterBuffer, body, dt, cost)   O(thrusters)
5. PHYSICS    body.integrate(dt)                                  O(1)
```

Steps 2 and 3 can be combined in a single pass through the network. The plasticity update uses activations computed in step 2.

> **Speed control and observation.** Because each `world.step(dt)` always advances by the same fixed `dt` regardless of wall-clock time, the simulation is trivially speed-controllable from the outside. The main loop uses a [fixed timestep accumulator](https://gafferongames.com/post/fix_your_timestep/) that drains accumulated real time in fixed-size chunks. To slow down for observation, cap how many steps drain per frame (`maxStepsPerFrame = 1` for real-time, uncapped for fast-forward). To pause, stop draining. None of this affects simulation determinism — the boid update pipeline above doesn't know or care how fast it's being called. See the Loose Coupling section in [plan_newplatform.md](plan_newplatform.md) for the full accumulator pattern.

### Network Activation (Pseudocode)

```cpp
void NeatNetwork::activate(const float* inputs, int nIn,
                           float* outputs, int nOut) {
    // Set input node activations
    for (int i = 0; i < nIn; i++)
        nodes[inputIds[i]].activation = inputs[i];

    // Propagate through hidden and output nodes (topological order)
    for (int nodeId : evaluationOrder) {
        float sum = nodes[nodeId].bias;
        for (auto& conn : incomingConnections[nodeId]) {
            if (!conn.enabled) continue;
            float w = conn.effectiveWeight();  // w + α×p
            sum += w * nodes[conn.sourceNode].activation;
        }
        nodes[nodeId].activation = activationFn(sum);
    }

    // Read output node activations
    for (int i = 0; i < nOut; i++)
        outputs[i] = sigmoid(nodes[outputIds[i]].activation);
        // sigmoid ensures [0,1] for thruster commands
}

void NeatNetwork::updatePlasticity(float dt) {
    for (auto& conn : allConnections) {
        if (!conn.enabled || conn.alpha == 0.0f) continue;

        float pre = nodes[conn.sourceNode].activation;
        float post = nodes[conn.targetNode].activation;

        // Check for neuromodulation
        float mod = 1.0f;
        if (conn.modulatorNode >= 0)
            mod = nodes[conn.modulatorNode].activation;

        float delta = conn.eta * mod * (
            conn.A * pre * post +
            conn.B * pre +
            conn.C * post +
            conn.D
        );

        conn.plasticWeight = std::clamp(
            conn.plasticWeight + delta * dt, -1.0f, 1.0f
        );
    }
}
```

---

## Evolution Mechanics

### NEAT Mutation Operators

Standard NEAT mutations apply to the network genes:

| Mutation | Rate | Effect |
|----------|------|--------|
| **Perturb weight** | ~80% of connections | Gaussian noise on w |
| **Replace weight** | ~10% of connections | Random new w |
| **Add connection** | ~5% per genome | New link between unconnected nodes |
| **Add node** | ~2% per genome | Split existing connection, insert hidden node |
| **Toggle connection** | ~1% per genome | Enable/disable a connection |

### Plasticity Mutation Operators (Additional)

| Mutation | Rate | Effect |
|----------|------|--------|
| **Perturb α** | ~30% of connections | Small Gaussian change to plasticity coefficient |
| **Perturb η** | ~30% of connections | Small Gaussian change to learning rate |
| **Perturb A,B,C,D** | ~20% of connections | Small Gaussian change to Hebbian rule |
| **Zero α** | ~5% of connections | Set α to 0 (disable plasticity for this connection) |
| **Add modulatory node** | ~1% per genome | Insert a modulatory neuron |

The lower rates for plasticity parameters reflect that they're a higher-level feature — should change more slowly than raw weights.

### Crossover

NEAT's innovation-number system handles crossover naturally, even with variable topology:

```
Parent A (more fit):  [1]--[2]--[3]--[5]--[7]--[8]
Parent B (less fit):  [1]--[2]--[4]--[5]--[6]

Offspring:            [1]--[2]--[3]--[5]--[7]--[8]
                       ↑    ↑    ↑    ↑    ↑    ↑
                      A/B  A/B   A    A/B   A    A
                     (match)(match)(disjoint)(match)(excess)(excess)
```

Matching genes (same innovation number): randomly inherit from either parent.
Disjoint/excess genes (only in one parent): inherit from the more fit parent.

For each inherited connection gene, all parameters come together: w, α, η, A, B, C, D. The plasticity parameters are part of the connection gene, so crossover handles them automatically.

### Speciation

NEAT groups similar organisms into species using a compatibility distance:

```
δ = c₁ × E/N + c₂ × D/N + c₃ × W̄
```

Where E = excess genes, D = disjoint genes, W̄ = mean weight difference of matching genes, N = max genome size, and c₁, c₂, c₃ are tunable coefficients.

We could extend this to include plasticity parameter differences:

```
δ = c₁ × E/N + c₂ × D/N + c₃ × W̄ + c₄ × P̄
```

Where P̄ = mean plasticity parameter difference. This ensures boids with similar learning strategies are grouped together, giving novel plasticity configurations time to optimise.

### Population Parameters (Starting Values)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Population size | 150–200 | Per species (predator/prey separate) |
| Species target | 8–15 | Adjust compatibility threshold to maintain |
| Survival rate | 25% | Top quarter of each species breeds |
| Elitism | 1 per species | Best organism always survives |
| Selection | Tournament, k=2 | Within species |
| Compatibility threshold (δ_t) | 3.0–6.0 | Tune based on speciation dynamics |

---

## Relevant Theory and Precedents

### Karl Sims, "Evolving Virtual Creatures" (1994)

[Sims' SIGGRAPH paper](https://www.karlsims.com/papers/siggraph94.pdf) is the closest precedent to what we're doing. He co-evolved **body morphology and neural control** in 3D block creatures that competed for resources. Key parallels:

- Body plan (our thruster positions) and brain (our processing network) evolved together
- Neural networks controlled "muscle" forces — analogous to our thruster commands
- Creatures specialised into distinct strategies through competitive co-evolution
- The genetic language encoded both morphology and neural circuitry

Sims used directed graphs for both body and brain representation, with crossover operating on graph fragments. NEAT's innovation system is a more principled solution to the same problem.

### Miconi et al., "Differentiable Plasticity" (2018)

[This paper](https://arxiv.org/abs/1804.02464) demonstrated that evolved Hebbian plasticity allows networks to:

- Memorise and reconstruct novel images not seen during training
- Solve meta-learning tasks competitively
- Outperform non-plastic networks in maze exploration RL tasks

Their key insight: the `α` parameter per connection — controlling how much the plastic component contributes — lets evolution decide *which connections should learn* and which should stay fixed. Networks converge on sparse plasticity patterns: most connections stay genetic, a few are highly plastic.

This directly supports the architecture above. We should expect evolution to discover that some connections (e.g., innate reflexes like "see predator → thrust away") work best as fixed weights, while others (e.g., "learn which direction predators usually come from in *this* environment") benefit from plasticity.

### Soltoggio et al., Evolved Neuromodulation

[Soltoggio's work](https://andrea.soltoggio.net/data/papers/SoltoggioALife2008.pdf) on integrating modulatory neurons into NEAT showed that:

- Evolution spontaneously inserts modulatory neurons when the task benefits from context-dependent learning
- Modulatory circuits improve performance in changing-reward tasks (like a predator that needs to adapt pursuit strategy)
- The biological parallel to dopamine/serotonin gating is computationally sound

### NEAT for Small Control Networks

[Stanley & Miikkulainen (2002)](https://nn.cs.utexas.edu/downloads/papers/stanley.ec02.pdf) showed that for problems in the 3–10 input range, NEAT finds solutions with significantly fewer evaluations than fixed-topology methods. The no-growth variant (fixed topology, evolved weights only) found solutions only 20% of the time given 1000 generations, taking 8.5x more evaluations when it did succeed.

For our boid brains (5–8 inputs, 3–4 outputs), NEAT is well within its sweet spot. Networks will likely stabilise at 10–25 total nodes and 20–50 connections — small enough to evaluate in microseconds.

---

## Implementation: Build and Test Sequence

We've decided to write our own NEAT. The question now is: what's the sequence of small, testable steps that gets us from "sensors produce floats" and "thrusters accept floats" (both already working) to "evolved brains drive boids"?

### Do We Need a Non-NEAT Stepping Stone?

Short answer: **not really, but a minimal smoke test is useful.**

NEAT at generation 0 *is* the simplest possible network — direct input→output connections with no hidden layer. It's literally Option A (fixed topology, evolved weights) except the topology can *later* grow. So there's no need to build a separate "simple network" system that we'd then throw away.

But there's still value in a smoke-test step before NEAT: a **hardcoded weight matrix** that proves the plumbing works. This is not a "non-NEAT brain" — it's a test fixture, like the `apply_random_wander()` function that currently drives the boids. The sequence is:

1. Get the `ProcessingNetwork` interface compiling with a trivial implementation
2. Verify that sensor outputs flow through it and drive thrusters correctly
3. *Then* build NEAT behind that same interface

This way, if something goes wrong with NEAT, we know the plumbing is sound.

### The Build Steps

Each step produces testable code. No step requires more than ~100–150 lines of new code. Tests run after every step.

---

#### Step 1: ProcessingNetwork Interface + Direct-Wire Smoke Test

**What:** Define the `ProcessingNetwork` abstract interface and implement a trivial `DirectWireNetwork` that maps sensor outputs straight to thruster inputs with a fixed weight matrix.

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

**Test:** Hand-set weights so that the forward sensor drives the rear thruster. Place two boids in a world, step the simulation, verify that the boid with a brain-driven thruster moves toward the detected boid. This is the first time sensors → brain → thrusters → physics runs end-to-end.

**Also test:** Edge cases — more sensors than thrusters, fewer sensors than thrusters, all-zero inputs, all-one inputs.

**Replaces:** `apply_random_wander()` in `App` with brain-driven control for boids that have a `ProcessingNetwork`.

---

#### Step 2: NEAT Genome Data Types

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

**Test:**
- `NeatGenome::minimal(7, 4, ...)` produces 11 nodes (7 input + 4 output) and 28 connections (fully connected), each with a unique innovation number
- Nodes have correct types
- Connections link inputs to outputs only (no input→input, no output→output)

**No network activation yet** — this is pure data.

---

#### Step 3: Network Activation (Phenotype from Genotype)

**What:** Build a `NeatNetwork` (the phenotype) from a `NeatGenome` (the genotype). This is the runtime network that actually computes `inputs → outputs`.

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

The key algorithm here is **topological sorting** of the network graph. For a minimal genome (no hidden nodes), this is trivial — outputs depend only on inputs. As hidden nodes are added later, the sort handles arbitrary DAGs. Recurrent connections (cycles) are detected and deferred to the next activation step.

**Test:**
- Minimal genome (7→4): set all weights to 0 → outputs are sigmoid(0) = 0.5 for all thrusters
- Set one weight to a large positive value → corresponding output saturates toward 1.0
- Set one weight to a large negative value → corresponding output saturates toward 0.0
- Manually construct a genome with one hidden node → verify it activates correctly (input → hidden → output, two-layer propagation)
- Compare `NeatNetwork` output with `DirectWireNetwork` output on the same weight matrix — they should match for the minimal (no-hidden-node) case

**This is the moment NEAT-at-generation-0 becomes equivalent to DirectWireNetwork.** The smoke test from Step 1 and the NEAT network should produce identical results given identical weights.

---

#### Step 4: Integration — Brain-Driven Boids in the World

**What:** Wire `NeatNetwork` into `Boid` so that `World::step()` runs the sense→think→act pipeline.

Currently, `Boid` has `std::optional<SensorySystem> sensors` and `std::vector<float> sensor_outputs`. We add `std::unique_ptr<ProcessingNetwork> brain`. The world step becomes:

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

**Test:**
- Create a boid from `simple_boid.json`, attach a minimal `NeatGenome`, build its `NeatNetwork`. Step the world. Verify thrusters receive non-zero power (sigmoid(0) = 0.5 for all, since initial weights are 0).
- Hand-tune weights so the front sensor drives the rear thruster. Place a target boid ahead. Verify the boid accelerates toward it.
- Verify that boids *without* a brain (no `ProcessingNetwork`) still work — `apply_random_wander()` or no thrust. Backwards compatibility with the existing demo.

**This is the full pipeline working for the first time.** From here, everything is about making the brains *better* through evolution.

---

#### Step 5: Mutation Operators

**What:** Implement the NEAT mutation functions that modify a genome.

| Operator | What It Does |
|----------|-------------|
| `mutate_weights(genome, rng)` | Perturb or replace connection weights (Gaussian noise) |
| `mutate_add_connection(genome, rng, innovation_tracker)` | Add a new connection between two unconnected nodes |
| `mutate_add_node(genome, rng, innovation_tracker)` | Split an existing connection, insert a hidden node |
| `mutate_toggle_connection(genome, rng)` | Enable/disable a random connection |
| `mutate_delete_connection(genome, rng)` | Remove a connection (pruning — not in original NEAT but we want it) |

The **innovation tracker** is a global counter + lookup table: "has the mutation source→target been seen before this generation?" If yes, reuse the innovation number. If no, assign a new one. This is essential for meaningful crossover later.

**Test:**
- `mutate_weights`: verify weights change, verify they stay within bounds
- `mutate_add_connection`: verify a new connection appears with a new innovation number. Verify it doesn't create duplicates. Verify it works when the network is already fully connected (should be a no-op).
- `mutate_add_node`: verify the original connection is disabled, two new connections appear (source→new_node, new_node→target), the new node exists. Verify the new node has weight 1.0 on the incoming connection and the original weight on the outgoing connection (NEAT convention — preserves existing behavior).
- `mutate_toggle_connection`: verify enabled/disabled flips
- `mutate_delete_connection`: verify connection is removed. Verify orphaned nodes (no remaining connections) are cleaned up.
- Mutate a genome, build a `NeatNetwork` from the mutated genome, verify it still activates without crashing. (Structural validity.)

---

#### Step 6: Crossover

**What:** Combine two parent genomes into an offspring genome using NEAT's innovation-number alignment.

```cpp
NeatGenome crossover(const NeatGenome& fitter_parent,
                     const NeatGenome& other_parent,
                     std::mt19937& rng);
```

Matching genes (same innovation number): randomly pick from either parent.
Disjoint/excess genes: inherit from the fitter parent only.

**Test:**
- Two identical genomes → offspring is identical
- Parent A has connection [innovation=5] that Parent B lacks → offspring has it (A is fitter)
- Both parents have connection [innovation=3] → offspring gets it from one or the other (check over many trials that it's roughly 50/50)
- Crossover of genomes with different hidden nodes → offspring genome is structurally valid (no dangling connections, no duplicate innovations)
- Build `NeatNetwork` from crossover offspring → activates without crashing

---

#### Step 7: Speciation + Population Management

**What:** The evolutionary loop that manages a population of genomes across generations.

Components:
- **Compatibility distance** δ between two genomes (count disjoint/excess genes + mean weight difference)
- **Species assignment**: group genomes into species by δ threshold
- **Fitness sharing**: divide each genome's fitness by its species size (prevents one species from dominating)
- **Selection**: tournament selection within species
- **Reproduction**: crossover + mutation → offspring. Elitism (best genome per species survives unchanged).
- **Generation cycle**: evaluate all → speciate → select → reproduce → repeat

```cpp
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

**Test:**
- Create population of 50 from a minimal seed genome. Verify all are in one species initially.
- Assign random fitness. Advance one generation. Verify population size is preserved.
- Advance 10 generations. Verify species count > 1 (mutations should create diversity).
- **XOR benchmark** (NEAT's classic test): 2 inputs + 1 bias → 1 output. Fitness = accuracy on the 4 XOR cases. Verify that NEAT solves it within ~100 generations. This is the acid test — if our NEAT can't solve XOR, something is fundamentally wrong. (XOR requires at least one hidden node, so this tests structural mutation + speciation working together.)

---

#### Step 8: Food, Energy, and Prey Fitness

**What:** Add food to the world and give prey boids a reason to move.

This is where the simulation stops being a physics demo and becomes an evolutionary system. We need:

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

**Test:**
- Place food at a known position, place a boid nearby with a brain that fires the rear thruster. Step until the boid reaches the food. Verify energy increases.
- Verify food disappears when eaten and respawns.
- Verify boid with zero energy stops being active.
- Run a population of 50 prey for one generation (6000 ticks). Verify fitness values are non-zero and vary across individuals. Verify the best-fitness boid found more food than the worst.

**Why food before predators?** Prey need a fitness signal to evolve *toward* something. Without food, fitness is just "survive longer" which rewards inactivity. With food, fitness rewards movement, sensor use, and navigation — exactly the behaviors we want the brain to discover. Once prey can forage, we have a working evolutionary system. Predators layer on top of that.

---

#### Step 9: Prey Evolution — The First Real Test

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

---

#### Step 10: Predator Population + Co-Evolution

**What:** Add a separate predator population that co-evolves alongside prey.

**Predator mechanics:**
- Predators have their own boid spec (different sensors? different thrusters? or same layout, different brain)
- Predators gain energy by catching prey (position within `catch_radius`)
- Caught prey die (removed from that generation's simulation)
- Predator fitness = total prey caught (or total energy from prey)
- Predator metabolism is higher than prey (must hunt to survive)

**Co-evolution:**
- Two separate `Population` objects, each evolving independently
- Both share the same world during evaluation
- Prey fitness is affected by predators (dying early = less foraging time = lower fitness)
- Predator fitness is affected by prey behavior (smarter prey = harder to catch)
- This creates an arms race: prey evolve evasion, predators evolve pursuit, prey evolve better evasion...

**Test:**
- Run with predators present. Verify prey mean fitness drops compared to no-predator runs (predation pressure exists).
- Verify predator mean fitness increases over generations (they get better at catching prey).
- Over many generations, verify that prey evolve visibly different behavior when predators are present vs absent (evasion vs pure foraging).

---

#### Step 11+: Plasticity, Neuromodulation, and Beyond

Once co-evolution is working:

- **Add plasticity parameters** (α, η, A, B, C, D) to `ConnectionGene`, initially all zero. Let mutation discover non-zero values. Compare fitness trajectories with and without plasticity enabled.
- **Add neuromodulation** (modulatory node type). See if evolution discovers useful modulatory circuits.
- **Evolve sensor parameters** (currently fixed in JSON). Unlock center_angle, arc_width, max_range as evolvable genes. See if prey and predators evolve different sensory layouts.
- **Evolve thruster layout** (the "mutable" flag from boid_theory.md). See if body plans diverge.

Each of these is its own evolutionary experiment, built on the working foundation from Steps 1–10.

---

### Relationship to the Build Planner

The detailed build steps in this section have been transferred to [step_by_step_planner.md](step_by_step_planner.md) (Phases 4, 5, and 5b), which is the single source of truth for "what to build next." This document remains the *design rationale* — the "why" behind each architectural decision. Refer to the planner for the current build sequence and file lists.

---

## Open Questions

1. **Should predator and prey share the same network architecture?** NEAT handles this naturally (they'd be separate populations evolving independently), but should they start from the same initial topology?

2. **Recurrent connections — yes or no?** NEAT can discover them. They provide memory (a neuron's output feeds back to itself or a predecessor). For boids, this could enable "I was recently alarmed" states. But recurrent networks are harder to reason about and can oscillate. Allow NEAT to discover them, but monitor for instability.

3. **Plasticity reset on reproduction?** When a boid reproduces, should the offspring inherit the parent's current plastic weights, or start with p=0? Biologically, learned traits aren't inherited (Lamarckian inheritance is mostly false). Starting at p=0 is more principled and prevents plastic weights from drifting into the genome's role. But inheriting some fraction of plastic state could accelerate adaptation in stable environments.

4. **How to evaluate plasticity's contribution?** Run comparative experiments: same population parameters, one with α forced to 0, one with evolved α. Compare fitness trajectories, behavioral complexity, and adaptation to environmental changes.

5. **Activation functions.** NEAT traditionally uses sigmoid. But for thruster control, we might want different functions: ReLU for hidden nodes (sparse activation), sigmoid for outputs (bounded [0,1] for thruster commands). NEAT can evolve the activation function per node if we want.

6. **Sensor parameters: co-evolved or pre-set?** Evolving sensor parameters (where to look, how far) alongside network topology is a lot of simultaneous adaptation. It might be easier to fix sensor configurations initially and evolve only the network, then unlock sensor evolution later. Similar to the locked/mutable thruster approach in [boid_theory.md](boid_theory.md).

---

## Summary of Recommendations

| Design Decision | Recommendation | Rationale |
|----------------|---------------|-----------|
| **Network architecture** | NEAT | Automatic topology discovery, starts simple, well-proven for agent control |
| **Plasticity** | Per-connection evolved Hebbian (ABCD model) | Enables lifetime learning, adds 6 genes per connection |
| **Neuromodulation** | Implement but enable later | Low cost, high potential, let evolution decide if it's useful |
| **Sensor → network interface** | Normalised float array | Clean abstraction, swappable sensors |
| **Network → thruster interface** | Sigmoid outputs in [0,1] | Natural mapping to thruster power |
| **Crossover** | NEAT innovation-number alignment | Handles variable topology cleanly |
| **Population structure** | Separate predator/prey populations, speciated within each | Co-evolution with protected innovation |
| **Implementation order** | Fixed weights → NEAT → plasticity → neuromodulation | Each phase validates the next |

---

## Sources

### NEAT
- [Evolving Neural Networks Through Augmenting Topologies (Stanley & Miikkulainen, 2002)](https://nn.cs.utexas.edu/downloads/papers/stanley.ec02.pdf)
- [NEAT Users Page](https://www.cs.ucf.edu/~kstanley/neat.html)
- [Efficient Evolution of Neural Network Topologies](https://nn.cs.utexas.edu/downloads/papers/stanley.cec02.pdf)
- [Successors of NEAT — Literature Review](https://direct.mit.edu/evco/article/29/1/1/97341/A-Systematic-Literature-Review-of-the-Successors)
- [NEAT C++ implementations](https://github.com/topics/neat?l=c++)

### Evolved Plasticity
- [Differentiable Plasticity (Miconi et al., 2018)](https://arxiv.org/abs/1804.02464)
- [Backpropamine: Neuromodulated Plasticity (Miconi et al., 2019)](https://openreview.net/pdf?id=r1lrAiA5Ym)
- [Meta-Learning through Hebbian Plasticity in Random Networks](https://proceedings.neurips.cc/paper/2020/file/ee23e7ad9b473ad072d57aaa9b2a5222-Paper.pdf)
- [Born to Learn: Evolved Plastic ANNs (Soltoggio et al., 2018)](https://www.sciencedirect.com/science/article/abs/pii/S0893608018302120)
- [Hebbian Theory — Wikipedia](https://en.wikipedia.org/wiki/Hebbian_theory)

### Neuromodulation
- [Evolutionary Advantages of Neuromodulated Plasticity (Soltoggio et al., 2008)](https://andrea.soltoggio.net/data/papers/SoltoggioALife2008.pdf)
- [The Inspiration, Progress, and Future of Evolved Plastic ANNs](https://arxiv.org/pdf/1703.10371)
- [Neuromodulation — Wikipedia](https://en.wikipedia.org/wiki/Neuromodulation)

### Agent Control and Virtual Creatures
- [Evolving Virtual Creatures (Karl Sims, 1994)](https://www.karlsims.com/papers/siggraph94.pdf)
- [Neuroevolution — Scholarpedia](http://www.scholarpedia.org/article/Neuroevolution)
- [Neuroevolution — Nature of Code](https://natureofcode.com/neuroevolution/)

### HyperNEAT / CPPNs
- [Compositional Pattern-Producing Networks — Wikipedia](https://en.wikipedia.org/wiki/Compositional_pattern-producing_network)
- [Deep HyperNEAT](http://web.mit.edu/fsosa/www/papers/dhn18.pdf)

### Comparative Studies
- [Topology vs Weight Evolution (Angeline et al., 1994)](https://link.springer.com/article/10.1023/A:1008272615525)
- [Weight Agnostic Neural Networks](https://weightagnostic.github.io/)

### Braitenberg Vehicles and Simple Sensor-Motor Mappings
- [Braitenberg Vehicles — Wikipedia](https://en.wikipedia.org/wiki/Braitenberg_vehicle)
- [From Braitenberg Vehicles to Neural Controllers (PMC)](https://pmc.ncbi.nlm.nih.gov/articles/PMC7525016/)
