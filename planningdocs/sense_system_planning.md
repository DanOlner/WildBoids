# BOID SENSE SYSTEM PLANNING

## Compound-Eye Sensors with Evolvable Morphology

**Goal:** Replace the current separate sensor arrays (7 boid-detecting + 3 food-detecting) with unified "compound eyes" — N physical eyes evenly or evolutionarily distributed around the boid, each reporting multiple detection channels. This unifies the sensor layout into a single evolvable structure and provides richer, spatially-correlated information to NEAT.

**Current problem with separate sensor arrays:**

The current design has boid-detecting sensors at one set of angles (5 narrow at ±72° forward, 2 wide at ±135° rear) and food-detecting sensors at completely different angles (3 wide at 120° spacing). This means:
- The boid literally "sees" food and other boids through different eye arrangements
- Evolving sensor directions (Option K follow-on) would require evolving two separate layouts
- NEAT can't easily learn "there's food AND a predator at my 2 o'clock" because those signals come from sensors with different spatial coverage
- Crossover between morphologies is messy — swapping a food sensor angle into a boid sensor slot changes what the NEAT wiring means

**Proposed design — unified multi-channel eyes:**

Each physical eye position produces up to 3 detection channels:
1. **Food channel** — nearest food distance (0–1, as current NearestDistance)
2. **Same-type channel** — nearest same-species boid distance (prey sees prey, predator sees predator)
3. **Opposite-type channel** — nearest opposite-species boid distance (prey sees predators, predator sees prey)

Plus 1 speed proprioceptive sensor (unchanged).

So with N eyes: `N × 3 + 1` total NEAT inputs.

**Key design choices:**

| Eyes | Channels | Total inputs | Initial NEAT connections (× 6 thrusters) |
|------|----------|-------------|------------------------------------------|
| 8    | 3        | 25          | 150                                       |
| 10   | 3        | 31          | 186                                       |
| 12   | 3        | 37          | 222                                       |

8 eyes × 3 channels is a reasonable starting point — comparable information density to the current 10 detection sensors but spatially unified.

**"Same-type" and "opposite-type" rather than "prey" and "predator":** The spec file doesn't need to know what species it belongs to. A prey boid's opposite channel detects predators; a predator's opposite channel detects prey. Same code, same spec, species-agnostic.

**Global channel toggles:** A sim_config setting controls which channels are active:

```json
"sensors": {
    "eyes": 8,
    "arcWidthDeg": 45,
    "maxRange": 100,
    "channels": ["food", "same", "opposite"]
}
```

Disabled channels output constant 0.0 to NEAT — the input nodes still exist (preserving genome compatibility) but carry no information. This means:
- Food-only evolution: `"channels": ["food"]` — same-type and opposite-type inputs are zeroed
- Add predators later: switch to `"channels": ["food", "opposite"]` — the dormant opposite-type wiring in evolved champions might even partially activate, giving a head start on predator avoidance
- Full co-evolution: `"channels": ["food", "same", "opposite"]`

**Phase 1 — fixed layout, multi-channel eyes (no morphology evolution):**

Replace the current separate sensor arrays with N evenly-spaced eyes, each producing 3 channels. This is a pure refactor of the sensor system with no evolutionary machinery changes:
- New `CompoundEyeSpec` replaces the per-sensor `SensorSpec` list
- `SensorySystem::perceive()` iterates eyes × channels instead of individual sensors
- Boid spec JSON changes from a list of individual sensors to eye count + arc + range
- NEAT input count derived from `eyes × active_channels + 1`
- Existing tests updated, new tests for multi-channel perception

**Phase 2 — evolvable eye directions (morphology genome):**

Add a "morphology genome" alongside the NEAT genome — a float array of `center_angle` per eye:
- **Mutation:** Gaussian perturbation of angles (sigma ~5–10°)
- **Crossover:** Per-eye blend or random parent selection, aligned by eye index
- **Speciation:** Could incorporate morphology distance into the NEAT compatibility metric, or keep separate
- **Fixed eye count:** NEAT input count stays constant within a run, so genome compatibility is preserved
- Arc width could also be evolvable (narrow vs wide per eye) but start with fixed uniform arcs

**Evolutionary dynamics this creates:**

- **Emergent specialisation:** Prey might pack eyes forward for precise food-mouth alignment (especially with the Option K mouth mechanic). Predators might spread eyes wider for prey scanning.
- **Co-evolutionary pressure on layout:** Predators approaching from blind spots select for rear coverage in prey. Prey clustering in predator blind spots select for wider predator vision.
- **Single layout to optimise:** Evolution discovers one arrangement that serves all detection needs simultaneously, creating genuine trade-offs (frontal food precision vs rear predator awareness).
- **Correlated spatial info for NEAT:** "Food at 2 o'clock AND predator at 2 o'clock" arrives on adjacent input nodes, enabling richer learned responses than separate sensor systems could provide.

**Interactions with other options:**
- **Option K (mouth):** Compound eyes with dense forward coverage + directional mouth = strong selection for precise oriented approach to food.
- **Option C (sensor experiments):** The multi-channel design subsumes most of what Option C proposed — instead of separate experiments with different sensor types, channels are built-in and togglable.
- **Options A/B (co-evolution):** Same-type and opposite-type channels are designed for co-evolutionary scenarios from the start.

**Effort:** Phase 1 (fixed layout, multi-channel) is moderate — mainly refactoring the sensor system and updating spec files. Phase 2 (evolvable morphology) is a larger addition requiring a new genome type, mutation/crossover operators, and integration with the population manager. Recommend implementing Phase 1 first and running evolution experiments before adding Phase 2.

**Sensor-actuator balance — how many eyes do we need?**

There's a theoretical and practical question about the relationship between sensor count (NEAT inputs) and thruster count (NEAT outputs). With 6 thrusters, do we need a minimum number of sensor inputs for evolution to discover complex coordinated movement?

[Ashby's Law of Requisite Variety](https://en.wikipedia.org/wiki/Variety_(cybernetics)) from cybernetics provides the theoretical framing: a controller must have at least as much variety (complexity) as the system it regulates. In our case, 6 independent thrusters can produce a rich space of forces and torques. If sensory input is too impoverished — say 3 food-only sensors feeding 6 thrusters — the network can't distinguish enough environmental states to learn differentiated thruster responses. The controller variety is bottlenecked by the sensor variety, not the network capacity.

Research in evolutionary robotics supports this in practice:

- [Nolfi & Floreano's foundational work](https://www.sciencedirect.com/science/article/abs/pii/S002002559900078X) showed that evolution tends to discover surprisingly simple solutions even with many sensors available — robots solving discrimination tasks with just two photoreceptors. Evolution naturally economises on sensors, **but only to the extent that the task allows**. Complex multi-actuator coordination requires richer sensory differentiation.

- [Miras et al. (2022)](https://pmc.ncbi.nlm.nih.gov/articles/PMC9577008/) found a correlation between phenotypic complexity and evolvability, noting that evolution often settles on compact phenotypes — but these compact solutions lack evolvability for more complex tasks. The implication: if sensors are too few, evolution finds a local optimum (e.g. "always thrust forward") but can't escape it toward richer behaviors.

- NEAT's [minimal-complexity growth strategy](https://nn.cs.utexas.edu/?neat=) means it starts with direct input→output connections and only adds hidden nodes when needed. With few inputs and many outputs, the initial fully-connected network has sparse information per output. With 3 inputs × 6 outputs = 18 initial connections, each thruster gets only 3 signals to work with. With 25 inputs × 6 outputs = 150 initial connections, each thruster sees a rich sensory panorama from the start, giving NEAT much more raw material for selection to act on before hidden nodes are even needed.

- [Co-evolution of sensors and controllers](https://www.researchgate.net/publication/228707125_Co-evolution_of_Sensors_and_Controllers) research showed that when morphology and control co-evolve, evolution discovers the appropriate balance between sensory and motor complexity for the task. Notably, even without penalty on sensor count, evolved robots often used fewer sensors than available — suggesting evolution finds the minimum sufficient sensory resolution for the actuator complexity at hand.

- [Neuroevolution produces more focused information transfer](https://pmc.ncbi.nlm.nih.gov/articles/PMC11757640/) than backpropagation-trained networks, with information compressed into smaller sets of relevant signals. This suggests evolved networks naturally create internal bottlenecks — but they need enough raw sensory variety to have something worth compressing.

**Practical implications for wildboids:**

| Config | Inputs | Outputs | Initial connections | Sensory richness per thruster |
|--------|--------|---------|--------------------|-----------------------------|
| Current (7 boid + 3 food + speed) | 11 | 6 | 66 | ~2 relevant signals each |
| 8 eyes × 1 channel (food only) | 9 | 6 | 54 | sparse |
| 8 eyes × 3 channels + speed | 25 | 6 | 150 | rich |
| 12 eyes × 3 channels + speed | 37 | 6 | 222 | very rich, slower early evolution |

The current 11-input / 6-output setup is probably near the lower bound for useful complexity with 6 thrusters. Moving to 8 eyes × 3 channels (25 inputs) roughly triples the initial connection count, which should give NEAT substantially more raw material for evolving differentiated thruster control — particularly for the strafe thrusters, which need lateral sensory information that the current sensor layout provides poorly.

The risk of too many inputs is slower early evolution (more connections to search over, more initial genome complexity). But NEAT's complexification-from-minimal approach mitigates this: it starts by finding useful direct sensor→thruster mappings and only adds hidden-node complexity later. The key insight is that **more inputs don't add hidden-node complexity — they add connection variety**, which is exactly what NEAT's initial search phase needs.

A reasonable heuristic: aim for at least 3–5× as many inputs as outputs. This gives each output node enough distinct sensory signals to learn differentiated responses, while keeping the initial genome manageable. For 6 thrusters, that suggests 18–30 inputs — right in the 8-eye × 3-channel range.

---

## Biological parallels for multi-scale sensing

The compound-eye system gives boids detailed spatial information at short-to-medium range (100 units). But real animals layer multiple sensing modalities at different ranges and resolutions. This section explores biological precedents that could inform additional longer-distance, "vaguer" senses for boids — ones that feel qualitatively different from the sharp segment-based compound eyes.

### The problem with uniform sensing

The current compound eyes are essentially binary at their limit: either something is within 100 units and you get a distance-graded signal, or it's invisible. This creates a hard perceptual horizon. In nature, almost nothing works this way — animals sense at multiple scales with gracefully degrading precision.

### Biological multi-scale sensing

**1. Lateral line / water vibration (fish, amphibians)**

Fish have a lateral line organ running along their body that detects pressure waves and water displacement. It gives a coarse, omnidirectional sense of "something moving nearby" without precise direction or identity. Key properties:
- **Omnidirectional** — no angular resolution, just "activity level nearby"
- **Movement-sensitive** — stationary objects are invisible; only moving things produce signals
- **Range degrades smoothly** — signal strength falls off with inverse-square of distance
- **Cheap** — passive, no energy cost, always on

*Wildboids analogue:* A single "vibration" input that sums nearby boid movement (weighted by inverse distance²). Long range but no directional info. Value might be: total kinetic energy of boids within 300-500 units, normalised. This gives a vague "busy area" signal without telling the boid *where* to go — it would need to correlate with compound-eye data to act usefully.

**2. Olfaction / chemical gradients (insects, sharks, salmon)**

Chemical sensing works over enormous distances but with very poor spatial resolution. A moth tracking pheromones doesn't know exactly where the source is — it samples concentration at its current location and performs upwind casting (zigzag flight) to follow the gradient. Key properties:
- **Scalar, not directional** — you sense concentration *here*, not direction to source
- **Very long range** — kilometres for moths, hundreds of metres for sharks
- **Time-delayed** — chemicals diffuse slowly, so the signal represents the recent past
- **Gradient-following requires movement** — you must move and compare concentrations over time to extract direction
- **Wind/current dependent** — the medium carries the signal, so it's not a straight line to the source

*Wildboids analogue:* A "scent" field that diffuses from food patches (or from clusters of boids). Each boid samples the local concentration — a single scalar value. To navigate by scent, the NEAT network would need to learn to compare current concentration with recent memory (which it can't do directly without recurrence, but it could learn a "move forward if scent is increasing" heuristic from the correlation between thrust and changing scent signal over consecutive ticks). This could work especially well if food patches are far apart relative to compound-eye range, creating a "scent-then-see" two-phase foraging strategy.

**3. Echolocation (bats, dolphins, oilbirds)**

Active sensing that gives range and angular information but at a cost. Key properties:
- **Active — requires energy emission** (vocalization, clicks)
- **Long range** — 50+ metres for bats, 100+ metres for dolphins
- **Returns range AND direction** — better spatial resolution than chemical or vibration
- **Energetically expensive** — producing and processing calls is costly
- **Reveals your position** — predators can eavesdrop on echolocation calls
- **Duty-cycled** — animals don't echolocate constantly; they ping and listen, adjusting rate to need
- **Cluttered environments reduce effectiveness** — echoes from multiple objects create confusion

*Wildboids analogue:* A "ping" action that costs energy and returns a summary of what's within a long radius (say 400 units). Could be a dedicated output node (thruster-like, 0–1) where the boid decides each tick whether to ping. Returns something coarser than compound eyes — maybe just 4 quadrant summaries (front/back/left/right) for each channel, or even just "nearest food distance" and "nearest boid distance" globally. The energy cost creates a genuine decision: ping frequently for better awareness, or save energy for thrust. Predators eavesdropping on pings would add another co-evolutionary pressure.

**4. Electroreception (sharks, rays, platypus, electric eels)**

Detects bioelectric fields generated by muscle activity. Key properties:
- **Passive** — detects fields generated by other animals' movement (no energy cost to sense)
- **Very short range for passive** (~1 metre for sharks detecting prey heartbeats)
- **Medium range for active** (electric fish generate fields, ~2 metres)
- **Works in total darkness / murky water** — complementary to vision
- **Detects living things specifically** — doesn't detect food/inanimate objects
- **Sensitive to movement** — muscle contractions generate stronger signals

*Wildboids analogue:* Interesting for predator-prey asymmetry. Predators might sense the *thruster activity* of nearby prey — the more a prey is thrusting (fleeing), the more detectable it is. This creates a stealth/speed dilemma: moving slowly makes you harder to detect but easier to catch. Could be a simple input: "total thrust output of boids within radius X, weighted by distance." No directional info, just intensity.

**5. Infrared pit organs (pit vipers, some boas, vampire bats)**

Thermal sensing that works like a crude second set of "eyes" but at a different wavelength. Key properties:
- **Directional** — pit organs have angular resolution, like a blurry camera
- **Medium range** — effective to ~1 metre for snakes (scales with prey thermal signature)
- **Detects warm-blooded prey specifically** — complementary to vision
- **Low angular resolution** — much blurrier than vision, more like "warm blob that way"
- **Passive** — no energy cost

*Wildboids analogue:* A second set of wider, fewer "eyes" with much longer range but coarser resolution. Maybe 4 quadrant detectors at 400-unit range, compared to 16 fine eyes at 100-unit range. These could detect different things — e.g. the long-range eyes might only see boids (not food), creating a "wide scan then narrow focus" two-stage detection.

### Design principles from biology

Several patterns emerge from these biological examples:

**Principle 1: Scalar vs. spatial** — The cheapest long-range senses give you a scalar (concentration, vibration level) without direction. Getting direction requires either multiple samples over time (olfaction + movement) or more expensive sensing (echolocation). This creates a natural hierarchy: cheap omnidirectional awareness → expensive directional information.

**Principle 2: Active sensing has costs** — Echolocation, electric field generation, and even head-scanning in vision all cost energy. This creates genuine behavioural decisions: when to invest in sensing. In wildboids, making some senses cost energy would add a new dimension to evolved strategies.

**Principle 3: Movement reveals and conceals** — Many senses detect movement preferentially (lateral line, electroreception, motion-sensitive vision). This creates speed/stealth trade-offs that don't exist when sensing is purely distance-based.

**Principle 4: Modalities complement, not duplicate** — Animals rarely have two senses that do the same job at the same range. Each modality fills a different niche in range/resolution/cost space. If we add long-range sensing, it should feel qualitatively different from compound eyes, not just "compound eyes but further."

**Principle 5: Temporal integration matters** — Chemical gradients, vibration patterns, and echolocation all benefit from memory — comparing current signals with recent past. Our feed-forward NEAT networks can't do this directly, but two mechanisms could help: (a) input nodes that encode *change* (delta from last tick), or (b) recurrent connections in NEAT (a significant architectural addition).

### Candidate implementations for wildboids (rough ranking by complexity)

| Sense | Range | Resolution | Cost | New inputs | Implementation complexity |
|-------|-------|-----------|------|-----------|--------------------------|
| **Vibration field** | 300–500 | Scalar (omnidirectional) | Free | 1 | Low — sum nearby boid speeds, inverse-distance weighted |
| **Scent/gradient** | World-wide | Scalar (local concentration) | Free | 1–2 | Medium — need a diffusion field updated each tick |
| **Long-range coarse eyes** | 300–400 | 4 quadrants | Free | 4–12 | Low — same as compound eyes but fewer/wider |
| **Thrust-detection** | 200 | Scalar | Free | 1 | Low — sum nearby thrust, distance weighted |
| **Echolocation ping** | 400 | 4 quadrants | Energy | 4–12 + 1 output | Medium — new output node, conditional sensing |
| **Delta/change inputs** | Same as compound eyes | Per-eye | Free | 49 (doubles input) | Low code, high NEAT cost |

The simplest addition that's most "biologically different" from compound eyes would be **vibration field** (1 scalar input, tells you "things are moving nearby but not where") plus **long-range coarse eyes** (4 wide quadrant detectors at 3–4× the compound eye range, giving rough directional info at distance). Together that's 5–13 new inputs for a qualitatively richer sensory world, without a huge increase in NEAT genome complexity.

The most *interesting* addition for co-evolution would be **echolocation** (active, costly, directional) or **thrust-detection** (passive, creates stealth/speed dilemma). These add behavioural dimensions that pure distance sensing can't.

---

## Phase 2: Evolvable Eye Morphology

### Overview

A morphology genome that evolves alongside the NEAT brain genome. Each boid carries two genomes: a NEAT genome (network topology + weights) and a morphology genome (eye positions + arc widths). Both undergo selection, crossover, and mutation each generation, but the morphology genome is a simple fixed-length float array — no topology changes, no innovation tracking.

**Key constraint:** NEAT input count is fixed within a run. Eye count per group doesn't change — only the angles and arc distributions evolve. This preserves genome compatibility across the entire population.

### Morphology genome structure

```cpp
struct MorphologyGenome {
    // Short-range eyes: N angles + N arc widths
    std::vector<float> short_range_angles;     // center angle per eye (radians)
    std::vector<float> short_range_arc_fracs;  // fraction of 360° budget per eye

    // Long-range eyes: M angles + M arc widths
    std::vector<float> long_range_angles;      // center angle per eye (radians)
    std::vector<float> long_range_arc_fracs;   // fraction of long-range budget per eye
};
```

The `arc_fracs` vectors are **pre-normalisation values** — raw floats that get normalised to sum to 1.0 before converting to actual arc widths. This means mutation can freely perturb any value without worrying about the constraint; the phenotype extraction step enforces it.

### Constraint system

**Short-range group — full-circle coverage:**

All short-range eye arcs must sum to exactly 360°. The morphology genome stores a fraction per eye, normalised at phenotype extraction:

```
arc_width[i] = (arc_frac[i] / sum(arc_fracs)) × 360°
```

With 16 short-range eyes, the default is 22.5° each. Evolution can redistribute — e.g. packing narrow eyes forward and wider eyes to the rear, or clustering dense coverage at specific angles.

Additionally, short-range eye angles are **not freely placed** — they are derived from the arc widths. Eyes tile the circle contiguously: eye 0 starts at a base angle, eye 1 starts where eye 0's arc ends, etc. The genome controls a single `base_rotation` parameter that rotates the entire tiling, plus the arc fractions that determine how the 360° is carved up. This guarantees full coverage with no gaps or overlaps.

```cpp
// Phenotype extraction for short-range eyes:
float total_frac = sum(short_range_arc_fracs);
float cumulative = base_rotation;  // single evolvable offset, radians
for (int i = 0; i < N; ++i) {
    float arc = (short_range_arc_fracs[i] / total_frac) * 2π;
    eyes[i].arc_width = arc;
    eyes[i].center_angle = cumulative + arc / 2;
    cumulative += arc;
}
```

**Long-range group — arc budget constraint:**

Long-range eyes share a configurable **total arc budget** (e.g. 100°). Each eye gets a fraction of that budget, normalised the same way:

```
arc_width[i] = (arc_frac[i] / sum(arc_fracs)) × budget
```

Unlike short-range eyes, long-range eye angles are **freely evolvable** — they can point in any direction independently. Overlapping coverage is allowed (and potentially useful — redundant coverage of a critical direction). Gaps are the norm (100° across 4 eyes means most of the circle is uncovered).

```cpp
// Phenotype extraction for long-range eyes:
float total_frac = sum(long_range_arc_fracs);
for (int i = 0; i < M; ++i) {
    float arc = (long_range_arc_fracs[i] / total_frac) * budget;
    eyes[i].arc_width = arc;
    eyes[i].center_angle = long_range_angles[i];  // free placement
}
```

**Configuration in sim_config.json:**

```json
"morphology_evolution": {
    "enabled": true,
    "short_range": {
        "eye_count": 16,
        "total_arc_deg": 360,
        "max_range": 100
    },
    "long_range": {
        "eye_count": 4,
        "total_arc_deg": 100,
        "max_range": 300
    },
    "mutation": {
        "angle_sigma_deg": 5.0,
        "arc_frac_sigma": 0.1,
        "angle_mutate_prob": 0.8,
        "arc_mutate_prob": 0.8
    }
}
```

Changing `eye_count` or channels between runs changes NEAT input count — requiring a fresh population (or a seed genome with the right input count). But within a run, these are fixed.

### Mutation

Simple Gaussian perturbation on the float arrays:

1. **Angle mutation** (long-range only — short-range angles are derived from arc tiling):
   - Per eye: with probability `angle_mutate_prob`, add N(0, σ_angle) to center angle
   - Wrap to [-π, π]

2. **Arc fraction mutation** (both groups):
   - Per eye: with probability `arc_mutate_prob`, add N(0, σ_frac) to arc fraction
   - Clamp to minimum (e.g. 0.1) to prevent degenerate zero-width eyes
   - Normalisation at phenotype extraction handles the constraint automatically

3. **Base rotation mutation** (short-range only):
   - With probability `angle_mutate_prob`, add N(0, σ_angle) to base rotation
   - Wrap to [-π, π]

No structural mutations (add/remove eye) — eye count is fixed per run.

### Crossover

Morphology crossover mirrors NEAT's parent-selection logic: the fitter parent's morphology is preferred, with blending from the other parent.

**Per-eye crossover** (independently for each eye index):
- With probability 0.5: inherit angle and arc fraction from fitter parent
- With probability 0.5: inherit from other parent

This is simpler than NEAT crossover because there's no topology alignment needed — the genome is a fixed-length array indexed by eye position.

**Alternative — interpolated crossover:**
- `child_angle[i] = lerp(parent1_angle[i], parent2_angle[i], t)` where t ~ U(0.3, 0.7)
- Produces intermediate morphologies rather than parent-copying
- Could be a configurable option; per-eye swap is the safer default (preserves proven eye arrangements intact)

### Integration with Population

Two approaches, in order of recommendation:

**Approach A — Embedded in Population (recommended):**

Extend `Population` to carry a parallel `std::vector<MorphologyGenome>` alongside `std::vector<NeatGenome>`. The existing selection and speciation logic applies to NEAT genomes as before. When `reproduce_from_species()` creates a child:

1. Select parents (same parents for both genomes — the brain and body co-evolve as a unit)
2. NEAT crossover → child brain genome (existing code, unchanged)
3. Morphology crossover → child morphology genome (new, simple per-eye swap)
4. NEAT mutation → mutate child brain (existing code, unchanged)
5. Morphology mutation → mutate child morphology (new, Gaussian perturbation)

Speciation uses NEAT compatibility distance only. Morphology doesn't contribute to speciation — the brain topology is the dominant factor in behavioural similarity. (This could be revisited if morphology diversity needs protecting, but start simple.)

Elitism copies both genomes unchanged.

```cpp
// Population gains:
std::vector<MorphologyGenome> morphologies_;

// advance_generation() unchanged in structure — just also copies/crosses/mutates morphologies
// alongside the NEAT genomes at each step
```

**Approach B — Separate MorphologyPopulation (more complex, more flexible):**

A standalone `MorphologyPopulation` that mirrors `Population`'s interface but manages float-array genomes. The headless runner coordinates both populations, ensuring the same individual index maps to both a brain and a body. This is cleaner in separation-of-concerns terms but adds coordination complexity (keeping indices aligned through selection, ensuring the same parents are chosen for both genomes).

**Recommendation: Approach A.** The morphology genome is simple enough (fixed-length float array) that embedding it in `Population` is straightforward. The key insight is that brain and body must co-evolve as a unit — splitting them into separate populations with independent selection would break the brain-body correspondence that evolution needs.

### Phenotype application

At boid creation time (in `run_generation()` and in the GUI), the morphology genome is decoded into concrete `EyeSpec` values:

```cpp
CompoundEyeConfig apply_morphology(
    const CompoundEyeConfig& base_config,
    const MorphologyGenome& morpho,
    const MorphologyEvolutionConfig& config
) {
    CompoundEyeConfig result = base_config;

    // Short-range: tile 360° using arc fractions + base rotation
    float total_frac = sum(morpho.short_range_arc_fracs);
    float cumulative = morpho.base_rotation;
    for (int i = 0; i < config.short_range.eye_count; ++i) {
        float arc = (morpho.short_range_arc_fracs[i] / total_frac) * 2π;
        result.eyes[i].arc_width = arc;
        result.eyes[i].center_angle = wrap_angle(cumulative + arc / 2);
        cumulative += arc;
    }

    // Long-range: distribute budget using arc fractions + free angles
    total_frac = sum(morpho.long_range_arc_fracs);
    float budget = deg_to_rad(config.long_range.total_arc_deg);
    for (int i = 0; i < config.long_range.eye_count; ++i) {
        float arc = (morpho.long_range_arc_fracs[i] / total_frac) * budget;
        result.long_range_eyes[i].arc_width = arc;
        result.long_range_eyes[i].center_angle = morpho.long_range_angles[i];
    }

    return result;
}
```

NEAT input count doesn't change — same number of eyes, same channels. Only the spatial arrangement changes.

### Champion persistence

Extend the BoidSpec JSON to include the morphology genome:

```json
{
    "compoundEyes": { ... },
    "genome": { ... },
    "morphologyGenome": {
        "baseRotation": 0.15,
        "shortRangeArcFracs": [1.2, 0.8, 1.1, ...],
        "longRangeAngles": [0.0, 1.57, 3.14, -1.57],
        "longRangeArcFracs": [1.0, 1.0, 0.5, 1.5]
    }
}
```

When loading a champion:
- If `morphologyGenome` is present: decode it and apply to the compound eye config before creating boids
- If absent: use the compound eye config as-is (backward compatibility)

When seeding a new population from a champion:
- Clone both the NEAT genome and the morphology genome
- Apply initial morphology mutations to create population diversity (same as NEAT weight mutation on population init)

### Visualisation

The GUI should show the evolved eye layout. Since the renderer already draws sensor arcs, this mostly works automatically — the eyes just have different angles/widths than the uniform default. Useful additions:

- Colour-code short-range vs long-range arcs (already done if using different ranges)
- Show the arc width numerically on hover/debug overlay
- A "morphology view" toggle that draws all eye arcs prominently to visualise the evolved layout

### Flexibility for different configurations

The system should handle these scenarios without code changes:

| Scenario | Config change | Effect |
|----------|--------------|--------|
| More long-range eyes | `long_range.eye_count: 6` | 6 eyes share the 100° budget (narrower each), 6 more NEAT inputs per channel |
| Wider long-range budget | `long_range.total_arc_deg: 180` | Same 4 eyes but each can be wider, more coverage |
| Fewer short-range eyes | `short_range.eye_count: 8` | 8 eyes tile 360° (45° default each), fewer NEAT inputs |
| Single-channel long-range | Change `channels` in compound eyes | Fewer NEAT inputs per eye, faster evolution |

The only constraint is that eye counts and channel lists are fixed for the duration of an evolution run (NEAT input count must be stable).

### Implementation steps

1. **MorphologyGenome struct** — data structure with serialisation (JSON round-trip)
2. **Phenotype extraction** — `apply_morphology()` function that converts genome → `CompoundEyeConfig`
3. **Mutation operators** — Gaussian perturbation with clamping and wrapping
4. **Crossover operator** — per-eye parent selection
5. **Population integration** — add `morphologies_` vector, wire into reproduce/advance
6. **Headless runner** — apply morphology when creating boids, pass morphology to champion saving
7. **Config parsing** — `morphology_evolution` section in sim_config.json
8. **Champion persistence** — save/load morphology genome in BoidSpec JSON
9. **Tests** — constraint satisfaction (arcs sum correctly), mutation bounds, crossover, round-trip serialisation
10. **GUI** — apply morphology when loading champions (automatic if eye config is rebuilt)

### Risks and mitigations

**Risk: Brain-body mismatch after crossover.** A brain evolved for forward-clustered eyes gets paired with rear-clustered eyes from the other parent. **Mitigation:** This is actually fine — it's equivalent to the "damage" that NEAT crossover already causes by mixing connection weights. Selection pressure quickly eliminates unfit brain-body combinations. The per-eye crossover (rather than wholesale genome swap) limits the disruption.

**Risk: Degenerate morphologies.** Evolution pushes all arc budget into one eye, leaving others at minimum width. **Mitigation:** The minimum arc fraction clamp prevents true degeneracy. And if a single wide eye outperforms many narrow ones, that's a legitimate evolutionary discovery — the constraint system allows it within bounds.

**Risk: Slow convergence with two genomes.** More things to search over = slower evolution. **Mitigation:** The morphology genome is tiny (tens of floats vs thousands of NEAT parameters). Its search space is small and smooth (continuous angles, no topology). Morphology should converge quickly relative to NEAT. Starting with small mutation sigma means early generations focus on brain evolution with near-default morphology, then morphology refines as brains stabilise.

---
