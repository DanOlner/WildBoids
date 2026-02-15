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

3. **Historical markings.** Every new gene (connection) gets a globally unique *innovation number*. This solves the alignment problem during crossover — genes with matching innovation numbers correspond to the same structural element and can be crossed over meaningfully.

4. **Speciation.** Networks are grouped into species by structural similarity. Individuals compete primarily within their species, protecting new innovations from being immediately outcompeted by established topologies.

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

## Implementation Phases

### Phase 1: Fixed Topology, No Plasticity

Get the plumbing working. Hand-design a simple feedforward network (sensors → one hidden layer → thrusters). Evolve weights only. Verify that:
- Sensory layer produces sensible values
- Network outputs drive thrusters correctly
- Physics responds properly
- Evolution produces *any* meaningful behavior

This is Option A from above, used as a debugging scaffold.

### Phase 2: NEAT (Topology + Weight Evolution)

Replace the fixed network with NEAT. Start from minimal topology (direct sensor→thruster connections, no hidden nodes). Verify that:
- NEAT discovers useful hidden nodes
- Speciation produces distinct behavioral strategies
- Predator and prey populations co-evolve meaningfully

This is the core system. Get it working well before adding plasticity.

### Phase 3: NEAT + Plasticity

Add the α, η, A, B, C, D parameters to connection genes. Seed initial populations with α=0 everywhere (no plasticity — equivalent to Phase 2). Let mutation introduce plasticity. Verify that:
- Some connections evolve non-zero α (plasticity is discovered)
- Plastic boids outperform non-plastic ones in at least some scenarios
- Plasticity is sparse (not all connections become plastic — that would suggest runaway)

### Phase 4: Neuromodulation (If Warranted)

Add modulatory neuron type. See if evolution discovers useful modulatory circuits. This is exploratory — it may not be needed for the simulation to be interesting, but it's the most biologically rich option.

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
