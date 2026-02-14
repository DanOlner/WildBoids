# Wild Boids Evolution: Theory and Design Considerations

**Purpose:** Starter document exploring approaches for evolving boid sensory systems and behaviors. This covers: (1) sensory system design, (2) genetic representation, and (2a) evolution mechanisms.

Claude-drafted, with a few notes and additions from me once made, using this prompt: "Let's move on to ideas for updating how I previously approached the boids' evolution. I think this breaks down into two parts. One: the boids' sensory systems - how the see the world around them, how they differentially react to their own kind vs the other kind (predator vs prey). Two: how that is represented in a genetic algorithm that can evolve. And maybe a 2a, since that will have to be designed concurrently: the evolution mechanism itself i.e. previously I randomly crossed 'genes' from any surviving boids (I think with longer-living boids having a higher prob of breeding, but can't remember). Please search for sources and synthesise them with ideas for this boids evolving project, and put the resulting summary into @evolution_theory.md. This only needs to be a starter answer, we'll be digging further."

---

## Part 1: Sensory Systems - How Boids Perceive the World

### Original Wild Boids Approach (2007)

The original implementation used **6 configurable fields of view (FOV)** per boid:
- 3 FOVs for detecting own kind (prey→prey or predator→predator)
- 3 FOVs for detecting other kind (prey→predator or predator→prey)

Each FOV was an **8-vertex polygon** that could evolve its shape, with associated behavioral parameters (speed, weight, relative angle of response).

### Modern Approaches from Literature

#### 1. Simple Parametric Sensors

The classic Reynolds boids model uses a [perception volume shaped like a sphere with a cone removed from the back](https://www.red3d.com/cwr/boids/), defined by:
- **Perception distance** (how far can I see?)
- **Field of view angle** (how wide can I see?)

This is computationally cheap but limited in expressiveness.

#### 2. Geometric/Zonal Sensors

More sophisticated models use [multiple concentric zones](https://en.wikipedia.org/wiki/Boids) with different behavioral responses:
- **Repulsion zone** (very close) → strong avoidance
- **Alignment zone** (medium distance) → match heading
- **Attraction zone** (far) → move toward

Each zone can have independent radii that evolve separately.

#### 3. Neural Network Sensors

[NEAT-based approaches](https://blog.lunatech.com/posts/2024-02-29-the-neat-algorithm-evolving-neural-network-topologies) feed raw sensory inputs into an evolving neural network:
- Inputs: distances, angles, velocities of detected agents
- Outputs: steering forces, speed changes
- The network topology itself evolves alongside weights

Research shows neural controllers can [successfully evolve alignment, cohesion, and separation behaviors](https://link.springer.com/article/10.1007/s12530-024-09651-z) when given appropriate sensory inputs and fitness functions.

### Design Questions for Wild Boids 2.0

1. **Keep polygon FOVs or simplify?**
   - Polygons are expressive but computationally expensive
   - Could use pie-slice sectors (defined by angle + radius) as a middle ground
   - Or: simple parametric sensors with more of them

2. **How many sensors?**
   - Original: 6 (3 per type)
   - Could be more/fewer, or let the number itself evolve

3. **What information per detection?**
   - Position (distance + angle)
   - Velocity (speed + heading)
   - Type (predator/prey)
   - Could add: age, size, genetic similarity

---

## Part 2: Genetic Representation

### The Core Challenge

How do we encode sensory rules as "genes" that can be:
- **Inherited** (passed from parent to offspring)
- **Crossed over** (combined from two parents)
- **Mutated** (randomly perturbed)
- **Expressed** (translated into behavior)

### Approach A: Direct Parameter Encoding (Original Wild Boids)

Each gene directly maps to a behavioral parameter:

```
Gene String:
[directionMomentum | speedInertia | FOV1_shape | FOV1_speed | FOV1_weight | FOV1_angle | ...]
     0.7               0.4          8 points      5.0          0.8          1.2 rad
```

**Pros:**
- Simple to implement
- Easy to understand what each gene does
- Direct mapping between genotype and phenotype

**Cons:**
- Fixed structure (can't evolve new capabilities)
- May require careful parameter scaling
- Crossover can disrupt co-adapted gene combinations

### Approach B: Neural Network (NEAT-style)

Encode a neural network's topology and weights:

```
Genome = List of:
  - Node genes (input, hidden, output neurons)
  - Connection genes (from, to, weight, enabled, innovation#)
```

**Pros:**
- [Can evolve arbitrarily complex behaviors](https://nn.cs.utexas.edu/downloads/papers/stanley.ieeetec05.pdf)
- Starts simple, adds complexity only when needed
- Well-studied, many implementations available

**Cons:**
- Black box (hard to interpret what evolved)
- More computationally expensive
- May be overkill for relatively simple predator-prey dynamics

### Approach C: Hybrid (Recommended for Wild Boids 2.0)

Use direct parameter encoding for sensor geometry, but allow evolved weights for behavioral responses:

```
Sensory Genome:
  - Sensor configurations (angles, distances, shapes)
  - Per-sensor response strengths

Behavioral Genome:
  - How to combine multiple sensor inputs
  - Speed/turning preferences
  - Type-specific reactions (flee predator vs. chase prey)
```

This preserves interpretability while allowing rich behavior evolution.

### What Parameters Should Be Evolvable?

Based on [research in genetic boid simulations](https://github.com/attentionmech/genetic-boids) and the original Wild Boids:

| Parameter | Description | Range |
|-----------|-------------|-------|
| `perception_radius` | How far each sensor sees | 10-200 |
| `perception_angle` | Width of each sensor cone | 0-2π |
| `response_strength` | How strongly to react | -1 to 1 |
| `response_angle` | Direction of reaction (toward/away/tangent) | 0-2π |
| `direction_momentum` | Turning smoothness | 0-1 |
| `speed_preference` | Baseline speed | 1-10 |
| `type_bias` | Different reactions to own kind vs other | varies |

### Predator vs Prey Considerations

Research on [predator-prey coevolution](https://dl.acm.org/doi/10.1162/106454698568620) shows that the two populations face different evolutionary pressures:

**Predators need to evolve:**
- Detection of prey at distance
- Pursuit/interception strategies
- Energy management (when to chase vs. conserve)

**Prey need to evolve:**
- Early warning (detect predators quickly)
- Evasion (unpredictable movement, using obstacles)
- Safety in numbers (flocking as defense)

This asymmetry means the genomes could/should be structured differently, or at least evaluated against different fitness criteria.

---

## Part 2a: Evolution Mechanism

### Original Wild Boids Approach

From [oldjavacode_summary.md](oldjavacode_summary.md):
- **Selection:** Cube-root biased toward older boids (`index = (random * cbrt(n))³`)
- **Crossover:** 50/50 random selection from each parent for each gene
- **Mutation:** Mentioned but not implemented
- **Trigger:** Death (starvation for predators, being eaten for prey)

### Modern Approaches

#### 1. Fitness Functions

[Multiple fitness formulations exist](https://www.researchgate.net/publication/326943190_On_Genetic_Algorithm_Effectiveness_For_Finding_Behaviors_In_Agent-based_Predator_Prey_Models):

| Fitness Type | How It Works | Pros/Cons |
|--------------|--------------|-----------|
| **Survival time** | Longer life = higher fitness | Simple, natural; but slow feedback |
| **Distance-based** | Prey: maximize distance to predators | Faster feedback; may not capture survival |
| **Resource-based** | Predators: food eaten; Prey: food found | Clear signal; requires resource system |
| **Reproduction success** | Number of offspring | Most Darwinian; requires longer runs |

The [original Wild Boids used implicit survival-based fitness](oldjavacode_summary.md) - older boids have survived longer, so they're more fit. This is elegant but can be slow to drive evolution.

#### 2. Selection Methods

| Method | Description | When to Use |
|--------|-------------|-------------|
| **Roulette wheel** | Probability proportional to fitness | Standard, works well generally |
| **Tournament** | Pick k random, select best | Adjustable selection pressure |
| **Rank-based** | Based on rank not absolute fitness | When fitness values vary wildly |
| **Age-based** | [Individuals expire after N generations](https://www.tutorialspoint.com/genetic_algorithms/genetic_algorithms_survivor_selection.htm) (note that GA intro website, handy) | Maintains diversity |
| **Cube-root (original)** | Heavily biases toward top performers | Strong selection pressure |

#### 3. Crossover Strategies

**Uniform crossover** (original Wild Boids): Each gene independently chosen from either parent
- Good for independent genes
- Can break up co-adapted gene clusters

**Single-point crossover**: Split genome at one point, take first half from parent A, second from parent B
- Preserves gene clusters
- May be too conservative

**Blending crossover**: For numeric values, take weighted average
- Creates intermediate phenotypes
- Good for continuous parameters

#### 4. Mutation

The original didn't implement mutation, but [research suggests it's important](https://en.wikipedia.org/wiki/Genetic_algorithm) for:
- Maintaining genetic diversity
- Escaping local optima
- Exploring parameter space

**Mutation strategies:**
- **Gaussian perturbation**: Add small random value (scaled by current value)
- **Random reset**: Occasionally replace gene with random value
- **Adaptive mutation**: Higher rates when population fitness stagnates

### Coevolution Considerations

[Predator-prey coevolution can produce "arms races"](https://dl.acm.org/doi/10.1162/106454698568620) where:
1. Prey evolve better evasion
2. Predators evolve better pursuit to compensate
3. Prey evolve even better evasion
4. ...and so on

**Challenges:**
- [Red Queen dynamics](https://en.wikipedia.org/wiki/Evolutionary_arms_race): Both populations run fast just to stay in place
- Cycling: Populations oscillate rather than improving
- Collapse: One population goes "extinct" (becomes too weak to drive the other's evolution)

**Solutions from literature:**
- **Hall of Fame**: Test against historical best opponents, not just current
- **Pareto coevolution**: Maintain diverse strategies that beat different opponents
- **Spatial structure**: Local neighborhoods allow strategy diversity ([research](https://ccl.northwestern.edu/2007/gecco2007.pdf))

---

## Recommendations for Wild Boids 2.0

### Sensory System
1. **Start with parametric sectors** rather than full polygons
   - Defined by: center angle, width, radius
   - Cheaper to compute, easier to visualize
   - Can upgrade to polygons later if needed

2. **3-5 sensors per boid** seems a good balance
   - Enough for differentiated responses
   - Not so many that genome becomes unwieldy

3. **Include velocity information** in detection
   - Original only used position
   - Knowing which way others are moving enables prediction

### Genetic Representation
1. **Use direct parameter encoding** for interpretability
2. **Group related genes** (all parameters for one sensor together)
3. **Normalize all values to 0-1 range** for cleaner crossover/mutation
4. **Consider predator/prey genome asymmetry**

### Evolution Mechanism
1. **Keep survival-time fitness** (it's elegant and worked)
2. **Add mutation** (Gaussian, ~5% rate, small magnitude)
3. **Use tournament selection** (more controllable than cube-root)
4. **Consider spatial breeding** (breed with nearby boids, not global pool)

### Future Exploration
- **NEAT integration**: If direct encoding proves too limiting
- **Speciation**: Allow divergent strategies to coexist
- **Environmental variation**: Changing conditions to prevent overfitting

---

## Sources

- [Craig Reynolds' Boids](https://www.red3d.com/cwr/boids/)
- [Boids Wikipedia](https://en.wikipedia.org/wiki/Boids)
- [NEAT Algorithm Explained](https://blog.lunatech.com/posts/2024-02-29-the-neat-algorithm-evolving-neural-network-topologies)
- [Emergence of Flocking Behaviors (2024)](https://link.springer.com/article/10.1007/s12530-024-09651-z)
- [Coevolving Predator and Prey Robots](https://dl.acm.org/doi/10.1162/106454698568620)
- [GA Effectiveness in Predator-Prey Models](https://www.researchgate.net/publication/326943190_On_Genetic_Algorithm_Effectiveness_For_Finding_Behaviors_In_Agent-based_Predator_Prey_Models)
- [Coevolution of Predators and Prey in Spatial Model](https://ccl.northwestern.edu/2007/gecco2007.pdf)
- [Genetic Boids Implementation](https://github.com/attentionmech/genetic-boids)
- [Prey-Predator Genetic Algorithm](https://github.com/Hemant27031999/Prey_Predator_Genetic_Algorithm)
- [Genetic Algorithms - Survivor Selection](https://www.tutorialspoint.com/genetic_algorithms/genetic_algorithms_survivor_selection.htm)
- [Evolutionary Arms Race](https://en.wikipedia.org/wiki/Evolutionary_arms_race)
- [ABED: Agent-Based Evolutionary Dynamics](https://www.sciencedirect.com/science/article/abs/pii/S0899825619301459)
- [Evolving Agent-Based Models](https://www.sciencedirect.com/science/article/abs/pii/S1877750315000320)
