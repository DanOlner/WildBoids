# Wild Boids Evolution: Theory and Design Considerations

**Purpose:** Starter document exploring approaches for evolving boid sensory systems and behaviors. This covers: (1) sensory system design, (2) genetic representation, and (2a) evolution mechanisms.

Claude-drafted, with a few notes and additions from me once made, using this prompt: "Let's move on to ideas for updating how I previously approached the boids' evolution. I think this breaks down into two parts. One: the boids' sensory systems - how the see the world around them, how they differentially react to their own kind vs the other kind (predator vs prey). Two: how that is represented in a genetic algorithm that can evolve. And maybe a 2a, since that will have to be designed concurrently: the evolution mechanism itself i.e. previously I randomly crossed 'genes' from any surviving boids (I think with longer-living boids having a higher prob of breeding, but can't remember). Please search for sources and synthesise them with ideas for this boids evolving project, and put the resulting summary into @evolution_theory.md. This only needs to be a starter answer, we'll be digging further."

---

## Part 1: Sensory Systems - How Boids Perceive the World

### Craig Reynolds' Original Boids (1987)

Reynolds' foundational paper ["Flocks, Herds, and Schools: A Distributed Behavioral Model"](https://dl.acm.org/doi/10.1145/37401.37406) (SIGGRAPH '87) established three steering rules that produce emergent flocking from purely local interactions.

#### The Three Rules (in priority order)

Reynolds defined these as a **priority hierarchy**, not equal-weighted forces:

1. **Collision Avoidance / Separation** (highest priority)
   - Steer to avoid crowding nearby flockmates
   - Addresses immediate spatial conflicts
   - Implementation: accumulate a repulsion vector from all flockmates within a "protected range", scaled by an `avoidfactor`

2. **Velocity Matching / Alignment** (medium priority)
   - Steer toward the average heading of nearby flockmates
   - Provides predictive collision prevention (maintaining separation over time)
   - Implementation: compute average velocity of visible neighbors, steer toward it scaled by a `matchingfactor`

3. **Flock Centering / Cohesion** (lowest priority)
   - Steer toward the average position of nearby flockmates
   - Keeps the flock together as a group
   - Implementation: compute center of mass of visible neighbors, steer toward it scaled by a `centeringfactor`

#### Prioritized Acceleration Allocation

Rather than simply summing weighted forces (which can cause opposing forces to cancel out), Reynolds used a **priority-based acceleration budget**:

- Each behavior generates an acceleration request (a 3D vector)
- Requests are processed in priority order
- Each request's magnitude is accumulated until the total exceeds maximum available acceleration
- The final request is trimmed proportionally to fit the budget
- Lower-priority behaviors go unsatisfied if higher-priority needs consume all available acceleration

This prevents the "cancellation problem" where, e.g., equal-and-opposite separation and cohesion forces would produce zero net movement.

#### Local Perception Model

Each boid perceives only a local neighborhood, defined by:

- **Distance threshold**: maximum detection range
- **Inverse-square distance weighting**: nearby flockmates have much stronger influence than distant ones. Reynolds tried linear weighting first but found inverse-square matched zoological observations that "a fish is much more strongly influenced by its near neighbors than by distant members"
- Flockmates outside this neighborhood are ignored entirely

The paper emphasizes that **localized perception is essential** for realistic flocking — global awareness produces qualitatively different (and less natural) aggregate motion.

#### Flight Model

- **Geometric flight**: incremental transformations — forward translation along local Z-axis, steering rotations (pitch/yaw)
- **Banking**: roll aligns local Y-axis with lateral acceleration direction (visual realism)
- **Momentum conservation**: boids maintain velocity tendencies, preventing instant direction changes
- **Viscous speed damping**: prevents unlimited acceleration
- **Speed limits**: minimum and maximum speed parameters

#### Obstacle Avoidance

Reynolds described two approaches:

1. **Force field model**: repulsion field emanating from obstacles — simple but fails when approaching perpendicular to the field
2. **Steer-to-avoid** (preferred): boid considers only obstacles intersecting its forward path, identifies the closest silhouette edge, and steers to pass one body length beyond it

#### Computational Complexity

Naive implementation is **O(n²)** (each boid checks all others). Reynolds proposed **spatial partitioning** (position-based lattice bins) to reduce neighbor lookups toward O(n).

### Key Later Extensions

Several researchers built on Reynolds' foundation:

- **Reynolds (1999)** — ["Steering Behaviors for Autonomous Characters"](https://www.red3d.com/cwr/steer/): Expanded the behavioral repertoire to include leader following, path following, unaligned collision avoidance, and wandering. This became a widely-used reference for game AI.

- **Delgado-Mata et al.** — Added **fear and pheromone transmission**: emotion spreads between agents via olfactory signals modeled as particles in a free-expansion gas. This introduced affective state as a flocking influence.

- **Hartman and Benes** — ["Autonomous Boids"](https://wiki.santafe.edu/images/3/3f/Hartman,_Benes_-_Autonomous_Boids.pdf): Introduced a **change of leadership** force complementing alignment. A leadership parameter controls the probability of a boid "breaking away" from the flock; others follow once the leader slows. When leadership = 0, the model reduces to standard Reynolds.

- **Predator extensions** — Various implementations add a [predator that boids must avoid](https://vanhunteradams.com/Pico/Animal_Movement/Boids-predator.html), typically as a high-priority repulsion force that overrides normal flocking when a predator is detected within range.

- **UAV/robotics applications** — Recent work combines boids with [reinforcement learning](https://www.mdpi.com/2075-1702/13/4/255) and control barrier functions for real-world multi-agent coordination with safety guarantees.

---

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

#### Observed in Wild Boids: Later Generations Aren't Always Better

In a 500-generation co-evolution run with net fitness, the best-performing champions (visually assessed for feeding accuracy, efficient thruster use, and predator-prey dynamics) came from around generations 25–35, not 400+. Several interacting factors explain this:

**Red Queen in practice.** A gen-400 prey champion has high fitness *against gen-400 predators* but may perform worse in isolation or against earlier predators. Both populations specialize against the opponent they're currently facing, not against a general challenge. The fitness numbers climb (relative to current opponents) but the absolute quality of behavior can plateau or regress.

**Cycling/oscillation.** Rather than monotonic improvement, the populations chase each other through strategy space: prey evolve strategy A → predators counter with B → prey abandon A for C → predators shift to D → prey rediscover something like A. The populations oscillate without necessarily climbing a fixed hill.

**Overfitting to current opponent.** With a single predator population and single prey population evaluating each generation against each other, both can overfit to specific strategies present right now. Early-generation champions may be more *general* foragers/hunters before specialization narrows the repertoire.

**Diversity loss under NEAT speciation.** Over hundreds of generations, species stagnate and get culled, reducing strategy diversity. Surviving species may converge on a local optimum narrower than what existed at gen 35.

**Net fitness efficiency trap.** Net fitness rewards minimizing spending, which can push evolution toward *too* conservative strategies — boids that save energy so aggressively they don't explore or react fast enough. Early generations may have struck a better balance between efficiency and activity.

### Hall of Fame and Cross-Testing: Design Ideas for Wild Boids

The core insight: evaluating genomes only against the current generation's opponents produces a noisy, shifting fitness landscape. Testing against a curated set of historical opponents provides a more stable signal and prevents the loss of general-purpose strategies.

#### 1. Champion Archive

Save champion genomes at regular intervals (already done via `--save-best` and `--save-interval`). The archive becomes a library of opponents at different evolutionary stages.

**What we already have:**
- `data/champions/champion_prey_genN.json` and `data/champions/predators/champion_predator_genN.json` saved periodically
- Each champion includes the full boid spec + NEAT genome, so it can be loaded and instantiated

**What we'd need to add:**
- A manifest or index of archived champions with metadata (generation, fitness score, fitness mode, config hash)
- Tagging: mark specific champions as "notable" (e.g. gen-35 prey, gen-25 predator) for use as benchmarks

#### 2. Hall of Fame Evaluation

During evolution, evaluate each genome not just against the current opponent population but also against a set of historical champions.

**Simple version — mixed opponents:**
- Each generation, fill some fraction of the opponent slots (e.g. 20–30%) with genomes drawn from the champion archive rather than from the current evolving population
- Prey population faces a mix of current-gen predators and historical champion predators
- This creates selection pressure for generality: a genome must perform well against diverse opponents, not just the current one

**Implementation sketch:**
- `run_generation()` already takes genome vectors for prey and predators
- Before calling it, replace some fraction of the predator genomes with archived champion genomes (loaded from JSON)
- The archived predators don't evolve — they're fixed opponents providing a stable fitness baseline
- Same in reverse for predator evaluation: some prey are historical champions

**Considerations:**
- What fraction of opponents should be historical? Too many and current-gen evolution stalls (opponents don't co-adapt). Too few and the stabilizing effect is negligible. Literature suggests 20–30% is a reasonable starting point.
- Should the archive grow indefinitely, or be curated? Options: fixed-size ring buffer (newest replaces oldest), or Pareto front (keep champions that beat different opponents).

#### 3. Cross-Testing / Tournament Mode

A separate evaluation mode (not during evolution) that pits any champion against any other:

**Round-robin tournament:**
- Take N prey champions and M predator champions from the archive
- Run every prey×predator combination for K ticks
- Produce a matrix of scores: how well does prey_i do against predator_j?
- Identifies which champions are genuinely strong (good against many opponents) vs. specialists (good only against their co-evolved opponent)

**Implementation approach:**
- Could be a mode of the headless runner: `--tournament --prey-dir data/champions/ --predator-dir data/champions/predators/`
- Load all champion specs from the directories, run the combinatorial evaluation, output a results matrix as CSV
- Each cell: net fitness of prey champion i against predator champion j (and vice versa for predator fitness)

**Uses:**
- Identify the most *robust* champion across opponents (the one with highest minimum or mean score across all opponents)
- Detect cycling: if gen-35 prey beats gen-400 predator but gen-400 prey loses to gen-35 predator, that's evidence of Red Queen oscillation
- Select seeds for future evolution runs: start from the most robust champion rather than the latest one

#### 4. Staged / Curriculum Evolution

Rather than a single long run, break evolution into stages with different opponent pools:

- **Stage 1 (gen 0–50):** Evolve prey against random/minimal predators. Goal: learn basic foraging.
- **Stage 2 (gen 50–150):** Introduce the best prey champion from stage 1 as a seed, evolve against increasingly competent predators (drawn from a separately evolved predator archive).
- **Stage 3 (gen 150+):** Full co-evolution, seeded from stage 2 champions, with hall of fame opponents mixed in.

This avoids the problem where early co-evolution is dominated by random noise (neither population can do anything useful yet, so there's no selection signal), while later stages benefit from the stabilizing effect of historical opponents.

#### 5. Fitness Aggregation Across Opponents

If evaluating against multiple opponents per generation, how to combine scores?

- **Mean fitness:** rewards generalists. Risk: a genome that's mediocre against everyone beats one that's excellent against most but terrible against one.
- **Minimum fitness:** rewards worst-case robustness. Very conservative — may prevent specialization.
- **Weighted mean:** weight recent opponents more heavily than ancient ones, balancing adaptation and generality.
- **Pareto ranking:** no single score — maintain a Pareto front of genomes that are non-dominated across opponent matchups. NEAT's speciation already provides some of this structure.

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

## Part 3: The Biology — Evolution of Collective Movement in Animals

Via [this prompt](https://github.com/DanOlner/WildBoids/blob/master/llm_convos/2026-02-25_1100_The_user_opened_the_file_UsersdanolnerCodeclaudewi.md#human-1) (and see the group selection prompt further down).

### Deep Evolutionary Origins

Collective movement is ancient. Fossils of the trilobite *Ampyx priscus* from Morocco, dating back **480 million years**, preserve strings of blind animals arranged in single-file lines, all facing the same direction and maintaining contact through their long spines. Lead researcher Jean Vannier concluded that "collective behavior is not a new evolutionary innovation that appeared a couple of million years ago" but rather "is much older, dating back to the first biodiversification events of animal life." These trilobites likely coordinated through physical contact via spines and chemical communication, despite lacking vision.

- Vannier, J. et al. (2019). [Collective behaviour in 480-million-year-old trilobite arthropods from Morocco](https://www.nature.com/articles/s41598-019-51012-3). *Scientific Reports*, 9, 14941.

### Why Did Collective Movement Evolve? Core Theories

#### 1. Anti-Predator Benefits (the primary driver)

Multiple distinct mechanisms operate:

**The Selfish Herd (Hamilton, 1971):** Individuals reduce their personal predation risk by positioning themselves among conspecifics. Each animal attempts to minimize its "domain of danger" — the area of ground closer to it than to any other individual (a Voronoi tessellation). Predators preferentially attack the nearest prey, so moving toward the center of a group reduces individual risk. This produces aggregation as an emergent consequence of selfish individual behavior, not group-level cooperation.

- Hamilton, W.D. (1971). [Geometry for the Selfish Herd](https://www.sciencedirect.com/science/article/abs/pii/0022519371901895). *Journal of Theoretical Biology*, 31(2), 295–311.

**The Dilution Effect:** As group size increases, per-capita risk of being targeted decreases simply through dilution.

**The Many-Eyes Hypothesis:** Larger groups have more individuals scanning for predators, distributing the vigilance burden and allowing each individual more feeding time.

- Pulliam, H.R. (1973). On the advantages of flocking. *Journal of Theoretical Biology*, 38, 419–422.

**The Confusion Effect:** Many moving targets in a group create sensory overload for predators, making it harder to single out and track individual prey. Studies on three-dimensional starling flocks have demonstrated that predator attack success decreases with increasing group size.

- [Confusion effect in starling flocks](https://royalsocietypublishing.org/doi/10.1098/rsos.160564). *Royal Society Open Science*.

**Empirical Integration:** Ioannou, Guttal & Couzin (2012) used live bluegill sunfish predators attacking computer-generated virtual prey to demonstrate that prey exhibiting coordinated collective motion had significantly higher survival than those moving independently — direct experimental evidence that **predators select for collective motion in prey**.

- Ioannou, C.C., Guttal, V. & Couzin, I.D. (2012). [Predatory fish select for coordinated collective motion in virtual prey](https://www.science.org/doi/10.1126/science.1218919). *Science*, 337, 1212–1215.

#### 2. Energy Savings

Collective movement provides significant hydrodynamic and aerodynamic benefits. Fish swimming in schools use **53% less energy** than solitary fish, and recover from high-speed swimming 43% faster. At higher speeds, metabolic savings reach **56–79%**. Critically, these savings do not require fixed positional arrangements — energy benefits arise even as fish move within the school.

- Marras, S. et al. (2024). [Energy conservation by collective movement in schooling fish](https://elifesciences.org/articles/90352). *eLife*, 12, RP90352.
- [Collective movement reduces locomotion costs in turbulence](https://journals.plos.org/plosbiology/article?id=10.1371/journal.pbio.3002501). *PLOS Biology*.

#### 3. Enhanced Foraging and Information Sharing

Groups find food faster than individuals. Social interactions allow animals to combine private and social information about resource locations. The optimal balance between social and individual information depends on environment structure: clustered environments favor strong social information use, while uniform environments favor individual search.

- [Social interactions drive efficient foraging](https://elifesciences.org/articles/56196). *eLife*.
- [Collective learning from individual experiences](https://royalsocietypublishing.org/doi/10.1098/rsif.2018.0803). *Royal Society Interface*.

#### Self-Organization from Simple Rules

A foundational insight: complex collective patterns emerge from simple individual behavioral rules. Couzin & Krause (2003) demonstrated that very different collective structures (swarms, polarized groups, rotating mills) self-organize from small adjustments to a single parameter — the radius over which individuals align with each other.

- Couzin, I.D. & Krause, J. (2003). [Self-Organization and Collective Behavior in Vertebrates](https://www.semanticscholar.org/paper/Self-Organization-and-Collective-Behavior-in-Couzin-Krause/7ff4729f29ac11d0baca19878761bb1cccd7d1d6). *Advances in the Study of Behavior*, 32, 1–75.
- Sumpter, D.J.T. (2006). [The principles of collective animal behaviour](https://pmc.ncbi.nlm.nih.gov/articles/PMC1626537/). *Philosophical Transactions of the Royal Society B*, 361, 5–22.

---

### The Edge Paradox: How Does Herding Get Selected For?

If peripheral individuals suffer higher predation, and every individual is "trying" to get to the center, there will always be losers on the edges. So how does herding evolve?

The resolution operates through **individual selection on movement rules**, not group selection:

1. **Selection on rules, not positions:** What evolves is the behavioral rule "move toward your nearest neighbor when threatened." Individuals carrying genes for this rule have, *on average across their lifetime*, lower domains of danger than individuals who do not aggregate at all. Even an individual sometimes stuck on the edge of a group has lower predation risk than an isolated individual.

2. **Dominance hierarchies sort positions:** More dominant, larger, or faster animals tend to secure central positions. The selfish herd does not require all individuals to benefit equally — it only requires that aggregating is better on average than not aggregating.

3. **Dynamic reshuffling:** In mobile groups, positions constantly change. An individual on the edge at one moment may be central the next. The stochastic nature of position means the average fitness of "joiners" exceeds that of "loners."

**Computational confirmation:** Reluga & Viscido (2005) demonstrated that natural selection of localized movement rules (considering only nearest neighbors) is sufficient to promote the evolution of selfish herd behavior.

- Reluga, T.C. & Viscido, S.V. (2005). [Simulated evolution of selfish herd behavior](https://www.sciencedirect.com/science/article/abs/pii/S0022519304005715). *Journal of Theoretical Biology*, 234(2), 213–225.

**Evolutionary GA model:** Wood & Ackland (2007) evolved selfish herd strategies using genetic algorithms with individual-based models under simulated predation and foraging pressure. Distinct aggregating strategies emerged, driven primarily by predator avoidance.

- Wood, A.J. & Ackland, G.J. (2007). [Evolving the selfish herd](https://pmc.ncbi.nlm.nih.gov/articles/PMC2169279/). *Proceedings of the Royal Society B*, 274(1618), 1637–1642.

**Empirical support:** De Vos & O'Riain (2010) manipulated domains of danger of Cape fur seal decoys and showed that sharks preferentially attacked decoys with larger domains of danger. Quinn & Cresswell (2006) showed that sparrowhawks preferentially targeted widely-spaced redshanks in flocks.

- De Vos, A. & O'Riain, M.J. (2010). [Sharks shape the geometry of a selfish seal herd](https://pmc.ncbi.nlm.nih.gov/articles/PMC2817263/). *Biology Letters*, 6(4), 489–491.
- Quinn, J.L. & Cresswell, W. (2006). [Testing domains of danger in the selfish herd](https://pmc.ncbi.nlm.nih.gov/articles/PMC1634896/). *Proceedings of the Royal Society B*, 273, 2521–2526.

**A complication — "Selfish Herders Finish Last":** In mobile groups (where the group itself is moving), individuals who selfishly push toward the center can actually disrupt group coordination. Romenskyy et al. (2022) found that in mobile animal groups, selfish herding behavior produces worse outcomes than cooperative movement rules.

- Romenskyy, M. et al. (2022). ['Selfish herders' finish last in mobile animal groups](https://royalsocietypublishing.org/rspb/article/289/1985/20221653/79590/Selfish-herders-finish-last-in-mobile-animal). *Proceedings of the Royal Society B*, 289, 20221653.

---

### Group Selection vs. Individual Selection

#### The Theoretical Distinction

**Individual selection** is the default Darwinian mechanism: heritable variation in traits produces differential reproductive success among individuals. Genes encoding traits that improve individual survival and reproduction increase in frequency. The logic is self-reinforcing — any mutant that improves individual fitness, even at a cost to group members, spreads because bearers outreproduce non-bearers. This creates a persistent problem for the evolution of cooperation: if altruism is heritable, non-altruists ("defectors") free-ride on altruists' contributions while paying none of the costs, and should always outcompete them within any group.

**Group selection** is the hypothesis that selection can also act *between* groups as competing units. The mechanistic claim is that groups vary in heritable traits, and some groups survive and reproduce differentially because of those traits. If groups with more cooperative members consistently outperform groups of selfish individuals, between-group selection can in principle counteract the within-group erosion of cooperation.

The key formal distinction is one of **variance partitioning**: for group selection to be effective, variance in fitness-relevant traits must be concentrated *between* groups (groups must reliably differ from each other), not merely *within* them. A further practical constraint is temporal: within-group selection (fast, happening every generation) must be overcome by between-group selection (slower, requiring groups to compete as units over longer timescales).

- [Group Selection — Wikipedia](https://en.wikipedia.org/wiki/Group_selection)
- [Altruism and Group Selection — Internet Encyclopedia of Philosophy](https://iep.utm.edu/altruism-and-group-selection/)

#### Historical Arc

**Darwin's ambiguity.** Darwin himself recognized a version of this problem. In *On the Origin of Species* (1859) he puzzled over the sterile castes of social insects — worker bees that sacrifice reproduction entirely. He suggested "selection may be applied to the family, as well as to the individual," leaving the door open to family- or group-level reasoning without specifying the mechanism.

- [Darwin's One Special Difficulty](https://pmc.ncbi.nlm.nih.gov/articles/PMC2665839/). *PMC*.

**Wynne-Edwards (1962): group selection as orthodoxy.** In *Animal Dispersion in Relation to Social Behaviour*, V. C. Wynne-Edwards argued that many animal behaviors function to regulate population density for the good of the group. Animals assess local population density via "epideictic displays" — communal gatherings conveying census information — and voluntarily restrain their own breeding when the group is at risk of overshooting food supply. Since any individual that ignored the signal and bred normally would outcompete restrainers, Wynne-Edwards argued this required group selection.

- [V. C. Wynne-Edwards — Wikipedia](https://en.wikipedia.org/wiki/V._C._Wynne-Edwards)

**Williams (1966): the gene-centred critique.** George C. Williams' *Adaptation and Natural Selection* delivered the standard refutation, establishing a methodological norm: invoke the lowest level of organization sufficient to explain any observed adaptation. His key arguments: (1) genes satisfy the requirements for a unit of selection (high heritability, fidelity of transmission, persistence across generations) while groups do not; (2) virtually every behavior attributed to group selection could be reinterpreted as individually adaptive; (3) parsimony — invoking group selection is unnecessary unless individual-level explanations genuinely fail. Williams' book, later popularized by Dawkins' *The Selfish Gene* (1976), shifted the field firmly toward gene- and individual-level explanations.

- [Adaptation and Natural Selection — Wikipedia](https://en.wikipedia.org/wiki/Adaptation_and_Natural_Selection)

#### Kin Selection and Inclusive Fitness: Hamilton's Resolution

While Williams was dismantling naive group selection, W. D. Hamilton (1963–1964) provided a different resolution — one operating at the level of the gene but producing outcomes superficially resembling group benefit.

The key insight: an individual shares genes with relatives. If a gene causes its bearer to perform a costly act that benefits a relative, the gene may still increase in frequency if enough copies of itself in relatives are thereby preserved. Hamilton formalized this as **Hamilton's rule: rb - c > 0**, where **r** is the coefficient of relatedness, **b** is the fitness benefit to the recipient, and **c** is the fitness cost to the actor.

**Inclusive fitness** is the accounting framework: instead of measuring only direct offspring, we count the total transmission of an individual's genes, including copies transmitted *via* relatives whose reproduction the individual has helped. The term "kin selection" (coined by Maynard Smith) describes the *process* — selection operates through differential survival of kin groups — but the unit being selected is still the gene, requiring no special group-level inheritance mechanism.

- [Hamilton's Rule and the Causes of Social Evolution](https://pmc.ncbi.nlm.nih.gov/articles/PMC3982664/). *PMC*.
- [Kin Selection — Wikipedia](https://en.wikipedia.org/wiki/Kin_selection)
- [Inclusive Fitness — Wikipedia](https://en.wikipedia.org/wiki/Inclusive_fitness)

#### Multilevel Selection: The Price Equation Framework

The apparent opposition was significantly reframed by George Price's mathematical work (early 1970s) and later by David Sloan Wilson and Elliott Sober's **multilevel selection (MLS) theory**.

The **Price equation** decomposes the total change in any trait frequency into two additive components:
- **Within-group selection**: the covariance between trait value and fitness *within* each group (typically negative for altruism — altruists lose out inside their group)
- **Between-group selection**: the covariance between group-average trait value and group-average fitness (positive for altruism if cooperative groups outperform selfish ones)

Altruism evolves when the positive between-group covariance outweighs the negative within-group covariance. Crucially, **this decomposition is mathematically equivalent to Hamilton's rule** when appropriate quantities are related. Relatedness (r) corresponds to the statistical association between actor and recipient genotypes — which arises from shared descent but can also arise from population structure or any mechanism that concentrates similar phenotypes in the same groups.

This equivalence underpins the modern synthesis position: kin selection and multilevel selection are **not competing empirical theories** but **alternative mathematical framings** of the same underlying process. The debate is partly about causal interpretation — which framing better captures the biological mechanism in a given case.

- [Price Equation — Wikipedia](https://en.wikipedia.org/wiki/Price_equation)
- [The Price Equation and Unity of Social Evolution Theory](https://pmc.ncbi.nlm.nih.gov/articles/PMC7133503/). *PMC*.
- [The Mathematics of Kindness (Price equation explainer)](https://plus.maths.org/content/mathematics-kindness). *Plus Maths*.

The controversy was reignited by Nowak, Tarnita, and E. O. Wilson's 2010 *Nature* paper, which argued that inclusive fitness theory has limited generality and that multilevel selection deserves rehabilitation — provoking a response from over 130 biologists defending inclusive fitness. This debate remains active, though most researchers treat the frameworks as complementary lenses.

- [The Evolution of Eusociality (Nowak, Tarnita, Wilson 2010)](https://www.nature.com/articles/nature09205). *Nature*.
- [Kin Selection, Group Selection: A Controversy Without End?](https://blog.oup.com/2015/01/kin-group-selection-controversy/). *OUP Blog*.

#### Application to Flocking and Herding

**The individual-selection view:** George Williams (1966) argued that "a fleet herd of deer" is really just "a herd of fleet deer" — apparent group-level adaptations are better explained as individual adaptations expressed in a social context. For flocking, each individual benefits from aggregation (dilution, confusion, information, energy savings), so no group-level selection is needed.

- Williams, G.C. (1966). *Adaptation and Natural Selection*. Princeton University Press.
- [Pinker, S. (2012). The False Allure of Group Selection](https://www.edge.org/conversation/steven_pinker-the-false-allure-of-group-selection). Edge.org.

**Multilevel selection theory:** David Sloan Wilson and Elliott Sober revived group selection under "multilevel selection," arguing that selection acts simultaneously at multiple levels. Wilson's summary: "Selfishness beats altruism within groups. Altruistic groups beat selfish groups. Everything else is commentary."

- Wilson, D.S. & Sober, E. (1998). *Unto Others: The Evolution and Psychology of Unselfish Behavior*. Harvard University Press.
- [Evolution for the Good of the Group](https://www.americanscientist.org/article/evolution-for-the-good-of-the-group). *American Scientist*.

**Current consensus:** The majority of behavioral biologists consider individual selection (including inclusive fitness/kin selection) sufficient to explain collective movement. Group selection remains a minority position. For flocking specifically, the individual-level benefits are strong enough that group-level selection, even if it operates, is not necessary to explain the phenomenon.

**Kin selection** (Hamilton's rule: rb > c) is largely **not required** to explain flocking. Groups of unrelated animals are held together by individual cost-benefit decisions, not altruistic sacrifice. Kin selection becomes relevant primarily for genuinely costly behaviors (alarm calls, sentinel behavior), not for aggregation per se — though kin structure can reinforce collective behavior when groups consist of relatives.

---

### Actual Genetics of Collective Behavior

Recent research has identified specific genes and genetic architectures underlying collective movement:

#### Zebrafish: 90 Genes Tested via CRISPR

Researchers at Harvard and Max Planck tested **90 different genes** in zebrafish using CRISPR, mutating one gene at a time. Three categories of mutant collective behavior emerged: "scattered" (reduced cohesion), "coordinated" (enhanced aligned schooling), and "huddled" (dense but disordered).

Two notable genes:
- **scn1lab** (ortholog associated with Dravet syndrome/autism in humans): mutation causes dispersed swimming.
- **disc1** (associated with schizophrenia in humans): mutation causes tight huddling.

The mechanism is remarkably simple: just two basic visual responses (relative visual field occupancy and global visual motion detection) are sufficient to account for emergent group behavior. Mutations perturb these individual-level reflexes, which alter collective behavior predictably.

- Tang, W. et al. (2020). [Genetic Control of Collective Behavior in Zebrafish](https://www.hsci.harvard.edu/news/genetic-control-collective-behavior-zebrafish). *iScience*, 23(3), 100942.
- Harpaz, R. et al. (2021). [Collective behavior emerges from genetically controlled simple behavioral motifs in zebrafish](https://pmc.ncbi.nlm.nih.gov/articles/PMC8494438/). *Science Advances*, 7(42).

#### Sticklebacks: The Eda Gene and Lateral Line

In threespine sticklebacks, the **Eda (Ectodysplasin) gene** — the same gene responsible for differences in armor plating between marine and freshwater populations — also shapes schooling behavior. Key findings:
- **Distinct genetic modules** control different aspects of schooling: tendency to school and body position within the school map to different genomic regions.
- A genetic link exists between schooling position and variation in the **neurosensory lateral line** (the organ fish use to detect water pressure changes from nearby movement).
- This suggests schooling behavior evolved in part through changes in **sensory systems**, not just behavioral rules.

- Greenwood, A.K. et al. (2013). [Genetic and neural modularity underlie the evolution of schooling behavior in threespine sticklebacks](https://pmc.ncbi.nlm.nih.gov/articles/PMC3828509/). *Current Biology*, 23(19), 1884–1888.
- [Fred Hutch: Why fish school — genetic link to social behavior](https://www.fredhutch.org/en/news/center-news/2016/06/why-fish-school-study-finds-genetic-link-stickleback.html).

#### Guppies: Schooling, Neuroanatomy, and Predation

Artificial selection experiments on guppies for schooling propensity reveal:
- Alignment and attraction behaviors show **high heritability** (upper end of the range for social behaviors).
- **Genes involved in neuron migration and synaptic function** play key roles.
- Guppies selected for higher schooling show changes in **brain morphology** that increase efficiency of sensory information relay.
- Schooling evolution drives corresponding changes in anti-predator behavior and neuroanatomy.

- Kotrschal, A. et al. (2023). [Evolution of schooling drives changes in neuroanatomy and motion characteristics across predation contexts in guppies](https://www.nature.com/articles/s41467-023-41635-6). *Nature Communications*, 14, 5838.
- Heathcote, R.J.P. et al. (2023). [Functional convergence of genomic and transcriptomic architecture underlies schooling behaviour in a live-bearing fish](https://www.nature.com/articles/s41559-023-02249-9). *Nature Ecology & Evolution*.

#### Broader Genetic Architecture

The genetic architecture of collective behavior involves multiple systems:
- Sensory system genes (lateral line, vision)
- Neural circuit genes (synaptic function, neuron migration)
- Genes affecting individual movement parameters (speed, turning rate)
- Genes involved in social responsiveness

- Davidson, J.D. et al. (2023). [A multi-scale review of the dynamics of collective behaviour](https://pmc.ncbi.nlm.nih.gov/articles/PMC9939272/). *Philosophical Transactions of the Royal Society B*.

---

### Summary: How Herding/Flocking Gets Selected For

1. **Primary driver:** Predator avoidance (selfish herd, dilution, confusion, many eyes).
2. **Secondary drivers:** Energy savings, enhanced foraging through information sharing.
3. **Genetic basis:** Heritable variation in sensory systems (lateral line, vision) and neural circuits, with specific genes identified in model organisms.
4. **No paradox for edge individuals:** Aggregating (even at the edge) is better on average than being solitary; dynamic position changes distribute risk; dominance hierarchies sort positions but do not prevent herding from evolving.
5. **Kin selection is not required** but can reinforce collective behavior in kin-structured groups.
6. **Group selection is not required** but multilevel selection may provide additional pressure in some contexts.

### Relevance to Evolved Boid Simulations

Several key takeaways for Wild Boids:

- **Predation pressure is essential:** Without predators (or a fitness proxy for predation), collective movement will not evolve. The Ioannou et al. (2012) study directly confirms this — predators are the selection mechanism.
- **Simple sensory rules suffice:** The zebrafish genetics research shows that just two visual reflexes produce collective behavior. This validates the sensor-based approach over complex cognitive models.
- **Sensory system evolution matters:** The stickleback work shows that changes in sensory organs (not just behavioral rules) drive schooling evolution. In Wild Boids, evolving sensor parameters (range, angle, sensitivity) is biologically realistic.
- **The "selfish herders finish last" result is interesting for mobile groups:** If Wild Boids prey are being chased, pure domain-of-danger minimization may not be optimal. Cooperative movement rules (alignment, cohesion) may emerge as superior strategies.
- **Energy savings could be an additional selection pressure** if the simulation models metabolic cost of movement, providing a non-predation reason for grouping to evolve.

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
- [Collective behaviour in 480-million-year-old trilobites (Vannier et al. 2019)](https://www.nature.com/articles/s41598-019-51012-3)
- [Hamilton (1971) — Geometry for the Selfish Herd](https://www.sciencedirect.com/science/article/abs/pii/0022519371901895)
- [Confusion effect in starling flocks](https://royalsocietypublishing.org/doi/10.1098/rsos.160564)
- [Ioannou et al. (2012) — Predatory fish select for coordinated collective motion](https://www.science.org/doi/10.1126/science.1218919)
- [Energy conservation by collective movement in schooling fish (eLife)](https://elifesciences.org/articles/90352)
- [Couzin & Krause (2003) — Self-Organization and Collective Behavior in Vertebrates](https://www.semanticscholar.org/paper/Self-Organization-and-Collective-Behavior-in-Couzin-Krause/7ff4729f29ac11d0baca19878761bb1cccd7d1d6)
- [Sumpter (2006) — The principles of collective animal behaviour](https://pmc.ncbi.nlm.nih.gov/articles/PMC1626537/)
- [Reluga & Viscido (2005) — Simulated evolution of selfish herd behavior](https://www.sciencedirect.com/science/article/abs/pii/S0022519304005715)
- [Wood & Ackland (2007) — Evolving the selfish herd](https://pmc.ncbi.nlm.nih.gov/articles/PMC2169279/)
- [De Vos & O'Riain (2010) — Sharks shape the geometry of a selfish seal herd](https://pmc.ncbi.nlm.nih.gov/articles/PMC2817263/)
- [Quinn & Cresswell (2006) — Testing domains of danger](https://pmc.ncbi.nlm.nih.gov/articles/PMC1634896/)
- [Romenskyy et al. (2022) — Selfish herders finish last](https://royalsocietypublishing.org/rspb/article/289/1985/20221653/79590/Selfish-herders-finish-last-in-mobile-animal)
- [Pinker (2012) — The False Allure of Group Selection](https://www.edge.org/conversation/steven_pinker-the-false-allure-of-group-selection)
- [Tang et al. (2020) — Genetic Control of Collective Behavior in Zebrafish](https://www.hsci.harvard.edu/news/genetic-control-collective-behavior-zebrafish)
- [Harpaz et al. (2021) — Collective behavior from genetically controlled behavioral motifs](https://pmc.ncbi.nlm.nih.gov/articles/PMC8494438/)
- [Greenwood et al. (2013) — Genetic modularity in stickleback schooling](https://pmc.ncbi.nlm.nih.gov/articles/PMC3828509/)
- [Kotrschal et al. (2023) — Evolution of schooling drives neuroanatomy changes in guppies](https://www.nature.com/articles/s41467-023-41635-6)
- [Heathcote et al. (2023) — Genomic architecture of schooling in guppies](https://www.nature.com/articles/s41559-023-02249-9)
- [Davidson et al. (2023) — Multi-scale review of collective behaviour dynamics](https://pmc.ncbi.nlm.nih.gov/articles/PMC9939272/)
- [Group Selection — Wikipedia](https://en.wikipedia.org/wiki/Group_selection)
- [Altruism and Group Selection — Internet Encyclopedia of Philosophy](https://iep.utm.edu/altruism-and-group-selection/)
- [Darwin's One Special Difficulty — PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC2665839/)
- [V. C. Wynne-Edwards — Wikipedia](https://en.wikipedia.org/wiki/V._C._Wynne-Edwards)
- [Adaptation and Natural Selection — Wikipedia](https://en.wikipedia.org/wiki/Adaptation_and_Natural_Selection)
- [Hamilton's Rule and Causes of Social Evolution — PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC3982664/)
- [Kin Selection — Wikipedia](https://en.wikipedia.org/wiki/Kin_selection)
- [Inclusive Fitness — Wikipedia](https://en.wikipedia.org/wiki/Inclusive_fitness)
- [Price Equation — Wikipedia](https://en.wikipedia.org/wiki/Price_equation)
- [The Price Equation and Unity of Social Evolution Theory — PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC7133503/)
- [The Mathematics of Kindness — Plus Maths](https://plus.maths.org/content/mathematics-kindness)
- [Nowak, Tarnita, Wilson (2010) — The Evolution of Eusociality](https://www.nature.com/articles/nature09205)
- [Kin Selection, Group Selection: A Controversy Without End? — OUP Blog](https://blog.oup.com/2015/01/kin-group-selection-controversy/)

---

## Part 4: Limitations of "Total Energy Gained" as Fitness

### Current Setup

Fitness is currently `total_energy_gained` — the cumulative energy a boid has eaten over the entire generation (6000 ticks). For prey this comes from food items; for predators, from catching prey. This is simple, directly measurable, and worked well enough to get evolution off the ground. But it has several structural limitations worth thinking through.

### 1. Spatial Competition for a Finite Resource

The world is 1300×1300 with 5 food patches of 20 items each (100 food items present at any time, replenished patch-by-patch as they're consumed). 150 prey boids compete for this food. The total energy available per generation is bounded by the food spawn rate — there's a ceiling on how much food can physically flow through the system.

**Consequences:**
- Once the population gets moderately good at finding food, fitness becomes a **zero-sum competition** among prey rather than an open-ended optimization. One boid eating more means another eating less.
- This means natural selection is really selecting for **competitive foraging ability** (getting to food before others) rather than foraging ability per se. These may select for different traits — speed and aggression vs. efficient searching.
- A boid that's excellent at finding food in isolation might score poorly because it's always surrounded by competitors who got there first.
- The fitness ceiling also means diminishing returns for evolution: once the population saturates the available food, further improvement in food-finding provides diminishing marginal fitness gains.

### 2. No Replacement of Dead Boids

When a boid dies (energy drops to zero, or eaten by predator), it stays dead for the rest of the generation. Its `total_energy_gained` is frozen at whatever it accumulated before death.

**Consequences:**
- A boid that dies at tick 1000 with 200 energy scores less than one that survives to tick 6000 with 300 energy, even if the dead boid was a more efficient forager per unit time. Survival duration is baked into the fitness measure whether we want it or not.
- This conflates **two different skills**: ability to find food and ability to not die. These are correlated but not identical — a boid could be great at eating but terrible at avoiding predators, or vice versa.
- With predators active, a prey boid's fitness depends heavily on whether it happened to be near a predator, which is partly stochastic. Two genetically identical boids can have very different fitness scores based on spatial luck.
- As boids die off during a generation, surviving boids face **reduced competition** for food in the later ticks. Late survivors get an inflating bonus simply from having fewer competitors — their per-tick energy gain rises as the population thins out.

### 3. No Steady-State Population Dynamics

In real ecosystems, dying animals are replaced by births, maintaining population pressure. The current model runs a fixed cohort from start to finish with no replenishment.

**Consequences:**
- The ecological dynamics shift dramatically over a generation: early ticks have intense competition (150 boids, limited food), late ticks may have sparse populations with abundant food.
- This means the **selection environment changes over the course of evaluation**. Early survivors are selected for competing in crowds; late survivors are selected for doing well in emptier worlds. These may favour different strategies.
- With predators, the predator-prey ratio changes over the generation as prey die off. Predators that eat efficiently early may starve later when few prey remain. This makes predator fitness partially dependent on the order in which prey happen to be caught.

### 4. Generation-Level Evaluation Masks Temporal Dynamics

Because fitness is a single cumulative number over the whole generation, it obscures *when* energy was gained.

**Possible issues:**
- A boid that gains energy steadily (good forager) looks identical to one that stumbled onto a food patch early and then did nothing useful.
- There's no penalty for **wasteful behavior** — a boid that burns energy on pointless high-speed movement but also happens to eat a lot scores the same as an efficient one with the same total.
- Net energy (energy gained minus energy spent) might be more meaningful than gross energy gained. Currently `total_energy_gained` is purely cumulative gross intake — it only goes up, never down. Metabolic drain and thrust costs are deducted from the separate `energy` balance (which determines death) but are completely invisible to fitness. So a boid that burns energy recklessly on high-speed movement pays no direct fitness penalty for that waste — only the indirect penalty of dying sooner if it runs out of energy.

### 5. Predator Fitness Has Compounding Issues

For predators, fitness = total energy from catching prey. This compounds the above problems:

- **Prey availability is endogenous**: a predator's fitness depends on how many prey are alive and where they are, which depends on all other predators' behavior. Two equally skilled predators get very different scores if one happens to be near prey clusters.
- **Diminishing prey**: as predators eat prey, remaining prey become scarcer, making later catches harder. Early-eating predators inflate their scores while depleting the resource for others.
- **Co-evolutionary coupling**: prey fitness depends on predator behavior and vice versa. A predator genome that scores well in generation N might score poorly in generation N+1 if prey have evolved better evasion — fitness is not a stable property of the genome.

### Possible Directions

These aren't proposals yet, just starting thoughts on what might address some of these limitations:

- **Rate-based fitness**: energy gained per tick alive, rather than total — decouples survival duration from foraging ability.
- **Multiple shorter evaluations**: run each genome through several shorter episodes with fresh starting conditions, average the results — reduces spatial luck.
- **Steady-state population**: replace dead boids mid-generation (with random or offspring genomes) to maintain population pressure throughout.
- **Net energy fitness**: total gained minus total spent — rewards metabolic efficiency.
- **Relative fitness**: rank boids against cohort rather than using absolute energy — handles the zero-sum problem.
- **Time-discounted fitness**: weight early energy gain less than late energy gain (or vice versa) to shape what temporal strategies are favoured.
- **Separate survival and foraging components**: explicitly multi-objective fitness, e.g. `α × energy_rate + β × survival_time`.
