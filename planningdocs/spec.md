# Wildboids Specification

Quick reference for working with the simulation.

## Headless Runner CLI (`wildboids_headless`)

Runs evolution without graphics. Outputs CSV to stdout, status/diagnostics to stderr.

### Config

| Flag | Default | Description |
|------|---------|-------------|
| `--config PATH` | `data/sim_config.json` | Sim config JSON (world, food, energy, NEAT params). CLI flags below override values from this file. |
| `--spec PATH` | `data/simple_boid.json` | Prey boid spec JSON (sensors, thrusters, physics). |
| `--predator-spec PATH` | *(none)* | Predator boid spec. **Providing this enables co-evolution mode.** Without it, runs prey-only. |

### Evolution (override config)

| Flag | Default | Description |
|------|---------|-------------|
| `--generations N` | from config | Number of generations to run. |
| `--seed N` | 42 | RNG seed for reproducibility. |
| `--population N` | from config | Prey population size. |
| `--predator-population N` | same as prey | Predator population size (co-evolution only). |
| `--ticks N` | from config | Simulation ticks per generation. |

### World (override config)

| Flag | Default | Description |
|------|---------|-------------|
| `--world-size N` | from config | World width and height (square). |
| `--food-max N` | from config | Max food items in the world. |
| `--food-rate F` | from config | Food spawns per second. |
| `--food-energy F` | from config | Energy per food item. |
| `--metabolism F` | from config | Energy lost per second (base metabolic cost). |
| `--thrust-cost F` | from config | Energy cost per unit thrust per second. |
| `--angular-drag F` | from config | Angular drag coefficient. |
| `--linear-drag F` | from config | Linear drag coefficient. |

### Output

| Flag | Default | Description |
|------|---------|-------------|
| `--save-best` | off | Save champion JSON whenever a new all-time best fitness is found. |
| `--save-interval N` | from config | Save champion every N generations. |
| `--output-dir PATH` | `data/champions` | Directory for saved prey genomes. Predator champions go to `{output-dir}/predators/`. |

### CSV Output

**Prey-only mode:** `gen,best_fitness,mean_fitness,species_count,pop_size,survivors`

**Co-evolution mode:** `gen,prey_best,prey_mean,pred_best,pred_mean,prey_species,pred_species,prey_survivors,pred_survivors`

### Examples

```bash
# Prey-only evolution
./build-release/wildboids_headless --generations 100 --save-best > log.csv

# Co-evolution with custom populations and world size
./build-release/wildboids_headless \
  --predator-spec data/simple_predator.json \
  --population 150 --predator-population 50 \
  --world-size 3000 --generations 50 --save-best > coevo.csv
```

---

## NEAT Parameters (`sim_config.json` → `"neat"` block)

Maps to `PopulationParams` in `src/brain/population.h`.

### Structural Mutation

| Parameter | Default | Description |
|-----------|---------|-------------|
| `addNodeProb` | 0.1 | Chance per genome of splitting a connection with a new hidden node. Higher = faster complexity growth, risk of bloat. |
| `addConnectionProb` | 0.1 | Chance per genome of wiring two previously unconnected nodes. Higher = denser networks, harder weight optimisation. |

### Weight Mutation

| Parameter | Default | Description |
|-----------|---------|-------------|
| `weightMutateProb` | 0.8 | Fraction of connections perturbed each generation. The main driver of fitness improvement. |
| `weightSigma` | 0.5 | Std dev of Gaussian weight perturbation. Larger = bigger jumps, less refinement. |
| `weightReplaceProb` | 0.1 | Chance a mutated weight is fully randomised instead of perturbed. Escapes local optima. |

### Reproduction

| Parameter | Default | Description |
|-----------|---------|-------------|
| `crossoverProb` | 0.75 | Chance offspring is a two-parent crossover vs single-parent clone+mutate. |

### Speciation

| Parameter | Default | Description |
|-----------|---------|-------------|
| `compatThreshold` | 0.5 | Max compatibility distance for same species. Lower = more species, more niches. Tighter than classic NEAT (3.0–6.0). |

### Selection

| Parameter | Default | Description |
|-----------|---------|-------------|
| `survivalRate` | 0.25 | Top fraction of each species eligible to breed. Lower = stronger selection pressure. |
| `elitism` | 1 | Best N genomes per species copied unchanged. Prevents losing the champion to mutation. |
| `maxStagnation` | 15 | Generations without fitness improvement before a species is culled. Frees slots for productive species. |
