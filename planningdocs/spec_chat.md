# NEAT Parameters in `sim_config.json`

Claude-drafted reference for the `"neat"` block in [sim_config.json](data/sim_config.json). These control how the NEAT evolutionary algorithm discovers and refines neural network brains for boids. Each parameter is described with its current value, what it does, and how changing it would affect evolution.

See [evolution_neuralnet_thrusters.md](evolution_neuralnet_thrusters.md) for the full design rationale, and [population.h](src/brain/population.h) for the `PopulationParams` struct where these land in C++.

---

## Structural Mutation

### `addNodeProb` — 0.03 (3% per genome per generation)

Probability that a genome gains a new hidden node each generation. When triggered, an existing enabled connection is disabled and replaced by two new connections passing through a new node (source→new→target). The incoming connection gets weight 1.0 and the outgoing inherits the original weight, so the network's behaviour is initially unchanged — the new node is a "pass-through" that evolution can later specialise.

**Impact:** This is the primary complexity dial. Higher values push networks toward more hidden nodes faster, enabling more nonlinear input combinations (e.g. "flee only when predator is close AND energy is low") but also increasing the chance of bloat — nodes that add parameters without improving fitness. Lower values keep networks simpler for longer, which is fine if the task can be solved with direct sensor→thruster mappings but limits the ceiling for complex behaviour.

At 3%, roughly 4–5 boids per generation (out of 150) will gain a node. This is the standard NEAT rate from the original paper. For our small networks (5–8 inputs, 3–4 outputs), this is conservative — networks grow slowly, which is appropriate since even one well-placed hidden node can qualitatively change what the network can compute (it's the difference between a linear classifier and a universal approximator).

**Too high (>10%):** Networks accumulate structural complexity before weights have time to optimise. Many hidden nodes with random weights degrade performance. Speciation pressure increases as genomes diverge structurally.

**Too low (<1%):** Evolution is effectively weight-only for a long time. Fine for simple tasks but can't discover conditional behaviours that require hidden nodes.

---

### `addConnectionProb` — 0.05 (5% per genome per generation)

Probability of adding a new connection between two previously unconnected nodes. The new connection gets a random weight drawn from a normal distribution.

**Impact:** This determines how densely interconnected the network becomes. New connections let information flow along paths that didn't exist before — a sensor that previously had no influence on a particular thruster can suddenly affect it. In combination with `addNodeProb`, this shapes the network's topology: nodes provide nonlinear processing capacity, connections provide the wiring.

At 5%, roughly 7–8 genomes per generation gain a new connection. This is deliberately higher than `addNodeProb` because adding a connection is a gentler structural change — it doesn't introduce a new processing stage, just a new information pathway. A network can absorb a new connection more easily than a new node.

**Too high (>15%):** Networks rapidly become fully connected, losing the sparse modularity that lets evolution create specialised sub-circuits. Every sensor affects every thruster, which sounds flexible but makes weight optimisation harder (higher-dimensional search space).

**Too low (<2%):** Networks stay sparse. Good connections that would improve fitness take many generations to appear by chance. Slows the discovery of useful sensor-thruster pairings.

---

## Weight Mutation

### `weightMutateProb` — 0.8 (80% of connections per mutation event)

When a genome undergoes weight mutation (which happens every generation for almost every genome), each connection has this probability of having its weight perturbed. This is the workhorse of evolution — the vast majority of generational improvement comes from weight tuning, not structural changes.

**Impact:** At 80%, most connections are adjusted each generation, with 20% left unchanged. This provides broad exploration of the weight space while preserving some stability. The alternative — mutating 100% of weights every generation — would be too disruptive, erasing good weight combinations before they can be refined.

**Too high (>95%):** Every connection changes every generation. Good weight configurations are never stable enough to refine. The fitness landscape becomes noisy — a genome's fitness fluctuates wildly between generations even without structural changes.

**Too low (<50%):** Evolution becomes sluggish. Many connections retain suboptimal weights for many generations because they keep dodging the mutation roll. Structural mutations (new nodes, connections) are wasted if the weights feeding into them aren't tuned.

---

### `weightSigma` — 0.5

Standard deviation of the Gaussian noise added to a weight when it's perturbed. The actual perturbation is: `weight += N(0, sigma)`. Since connection weights are unbounded (but sigmoid outputs clip the network's behaviour), this controls the step size of weight exploration.

**Impact:** This is the granularity of weight search. A sigma of 0.5 means ~68% of perturbations are within ±0.5 of the current weight, and ~95% are within ±1.0. Given that sigmoid saturates beyond about ±4, a single perturbation can meaningfully shift a connection's influence without completely randomising it.

**Too high (>1.5):** Weight perturbations are so large they effectively randomise the connection. A weight of 2.0 getting perturbed by ±3.0 could end up anywhere from -1 to 5. This destroys incremental refinement — evolution can't "hill-climb" toward better weights because each step overshoots.

**Too low (<0.1):** Weight changes are too small to escape local optima. A weight stuck at a bad value takes many generations of tiny nudges to reach a better region. Evolution converges prematurely on mediocre solutions. Fine-tuning works well but exploration is poor.

**Interaction with `weightReplaceProb`:** The sigma controls *perturbation* step size. `weightReplaceProb` provides an escape hatch — a small chance of jumping to an entirely new random weight. Together they give evolution both local refinement (sigma) and occasional long-range jumps (replace).

---

### `weightReplaceProb` — 0.1 (10% of mutated connections)

When a connection's weight is selected for mutation (per `weightMutateProb`), there's a 10% chance the weight is replaced entirely with a fresh random value (from a uniform or normal distribution) instead of being perturbed. This is the "hard reset" alternative to gentle perturbation.

**Impact:** Prevents weight stagnation. If a connection's weight has been perturbed into a local minimum (neither increasing nor decreasing fitness), perturbation alone can't escape because small steps in any direction are downhill. A full replacement teleports the weight to a random point in weight space, giving it a chance to find a better basin.

**Too high (>30%):** Too many connections lose their tuned values each generation. The population can't maintain good solutions because their weights keep getting scrambled. Effectively reduces the heritability of weight-based fitness.

**Too low (<3%):** Weights get stuck in local optima. The only way out is structural mutation (new nodes/connections that bypass the stuck weight), which is much rarer and slower.

---

## Reproduction

### `crossoverProb` — 0.75 (75%)

When producing an offspring for the next generation, there's a 75% chance it's created by crossing over two parents (combining their genomes using NEAT's innovation-number alignment), and a 25% chance it's created by cloning a single parent and mutating the clone.

**Impact:** Crossover combines successful traits from different lineages. A boid that's good at steering left and one that's good at detecting food can produce offspring with both abilities — but only if the relevant genes happen to land in the same offspring. Higher crossover rates increase recombination, which is valuable when the population has diverse solutions with complementary strengths.

At 75%, three-quarters of offspring are recombinations. This is a standard GA setting that balances exploration (new gene combinations) with exploitation (cloning known-good genomes and refining them).

**Too high (>90%):** Almost every offspring is a hybrid. Good genomes rarely get to pass on their full gene set intact (beyond the elite). If two parents have incompatible topologies (different hidden node arrangements), crossover can produce confused offspring that inherit conflicting sub-circuits.

**Too low (<40%):** The population becomes mostly clones-with-perturbation. Beneficial innovations in one lineage take a long time to spread to others. Evolution proceeds lineage-by-lineage rather than population-wide, which is slower.

---

## Speciation

### `compatThreshold` — 0.5

The compatibility distance threshold that determines whether two genomes belong to the same species. Two genomes with compatibility distance δ < 0.5 are considered the same species; δ ≥ 0.5 puts them in different species.

Compatibility distance is: `δ = c₁ × E/N + c₂ × D/N + c₃ × W̄` where E = excess genes, D = disjoint genes, W̄ = mean weight difference of matching genes, N = max genome size, and c₁, c₂, c₃ are the coefficients in `CompatibilityParams`.

**Impact:** This is the speciation sensitivity dial. Lower thresholds create more species (even small structural or weight differences split populations). Higher thresholds create fewer, larger species.

At 0.5, this is tighter than the original NEAT paper's default of 3.0–6.0, which means our population will fragment into more species. This is deliberate for a small population of 150 — more species means more protected niches for innovations to develop, at the cost of fewer individuals per species to drive within-species competition.

**Too high (>3.0):** Most genomes end up in one or two species. Innovations aren't protected — a genome with a new hidden node competes directly against established genomes with tuned weights and loses. The population converges on a single topology. This is the failure mode that speciation exists to prevent.

**Too low (<0.1):** Every tiny weight difference creates a new species. Species have 1–3 members each, which is too few for meaningful within-species selection. Elitism means every genome in a tiny species survives, eliminating selection pressure entirely. Fitness sharing becomes meaningless. The population drifts randomly rather than evolving.

**Tuning note:** Some NEAT implementations dynamically adjust this threshold to maintain a target species count (e.g. 8–15). If the species count is too high, increase the threshold; if too low, decrease it. Our current implementation uses a fixed threshold.

---

## Selection

### `survivalRate` — 0.25 (25%)

Within each species, only the top 25% by fitness are eligible to breed. The bottom 75% are discarded at each generation boundary (their genomes don't produce offspring).

**Impact:** This controls selection pressure — how aggressively evolution culls poor performers. A survival rate of 25% means only the best quarter of each species contributes genes to the next generation, which is strong selection pressure.

**Too high (>60%):** Weak selection pressure. Mediocre genomes reproduce alongside good ones, slowing fitness improvement. The population carries a lot of "dead weight" genes that persist because they're rarely eliminated.

**Too low (<10%):** Very aggressive selection. Only the very best of each species breeds, which drives rapid fitness improvement but at the cost of genetic diversity. If the top 10% happen to share a weakness (e.g. all turn right well but not left), the population loses the genes for the missing capability. Can cause premature convergence.

**Interaction with species size:** In a species with 4 members and survival_rate 0.25, only the top 1 breeds. With elitism = 1, that same individual is both the elite and the only parent. All offspring are mutations of the same genome — no crossover is possible. This is fine for small species (protects the innovation) but means genetic diversity within that species is zero.

---

### `elitism` — 1

Number of top genomes per species that are copied unchanged into the next generation. The best genome in each species always survives, with no mutation applied.

**Impact:** Elitism guarantees that the best solution found so far is never lost. Without elitism, the best genome in a species could be unluckily mutated into something worse, and the peak fitness achieved so far would be lost. This is critical for maintaining monotonically improving best-fitness trajectories.

At 1 per species, only the single best survives unchanged. This is the minimum useful elitism — it preserves the champion without flooding the next generation with copies of it.

**Higher values (2–5):** More safety margin — if the champion's fitness was a lucky fluke (e.g. spawned near food by chance), keeping the runner-up provides a backup. But in a species with only 6 members, 3 elites means half the species is unchanged clones, reducing exploration.

**Zero (no elitism):** Risky. The best genome is mutated like everything else. If the mutation is harmful, the population's best fitness drops. Over many generations, this creates a noisy fitness trajectory that never steadily improves. Almost all practical NEAT implementations use elitism ≥ 1.

---

## Stagnation

### `maxStagnation` — 15

A species is removed entirely if its best fitness hasn't improved for 15 consecutive generations. All genomes in the stagnant species are discarded, and their population slots are redistributed to other species.

**Impact:** This is the "give up on dead ends" mechanism. A species that was promising 15 generations ago but has plateaued is consuming population slots that could be used by more productive species. Removing it frees those slots for species that are still improving.

At 15 generations, a species gets a decent runway to explore. Structural mutations (new nodes/connections) can take 5–10 generations to have their weights optimised, so 15 generations gives enough time for a new topology to prove itself before being judged stagnant.

**Too high (>30):** Stagnant species linger too long, consuming population slots. In a 150-member population with 10 species, a stagnant species of 15 members means 10% of the population is doing nothing useful for 30 generations. That's 30 generations × 15 wasted evaluations = 450 wasted fitness evaluations.

**Too low (<5):** Species are culled before structural innovations have time to optimise their weights. A genome that just gained a hidden node has poor fitness for 3–4 generations while its weights adjust. If the species is killed at generation 5, the innovation never gets a chance. This effectively prevents topology growth — the same failure mode as not having speciation at all.

**Edge case:** The species with the population's overall best genome is never removed regardless of stagnation. This prevents losing the global champion due to species-level culling.
