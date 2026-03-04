#pragma once

#include "simulation/sensor.h"
#include <string>
#include <vector>
#include <random>

// Per-group morphology: angles and arc fraction distribution for one sensor tier.
// Both short-range and long-range groups use the same struct.
struct SensorGroupMorphology {
    std::vector<float> angles;      // center angle per eye (radians)
    std::vector<float> arc_fracs;   // relative arc fractions (normalised at phenotype extraction)
};

// Full morphology genome: one entry per sensor group.
// Parallel to the NEAT brain genome — carried by each individual.
struct MorphologyGenome {
    std::vector<SensorGroupMorphology> groups;  // one per sensor tier (e.g. short_range, long_range)
};

// Config for one sensor group's morphology evolution.
struct MorphologyGroupConfig {
    int eye_count = 8;
    float total_arc_deg = 360.0f;   // arc budget in degrees
    float max_range = 100.0f;       // detection radius (world units)
};

// Mutation parameters for morphology evolution (separate from NEAT params).
struct MorphologyMutationParams {
    float angle_sigma_deg = 5.0f;    // Gaussian sigma for angle perturbation (degrees)
    float arc_frac_sigma = 0.1f;     // Gaussian sigma for arc fraction perturbation
    float angle_mutate_prob = 0.8f;  // per-eye probability of angle mutation
    float arc_mutate_prob = 0.8f;    // per-eye probability of arc fraction mutation
    float replace_prob = 0.05f;      // chance of full replacement instead of perturbation
    float min_arc_frac = 0.1f;       // minimum arc fraction (prevents degenerate zero-width eyes)
};

// Full morphology evolution config.
struct MorphologyEvolutionConfig {
    bool enabled = false;
    std::vector<MorphologyGroupConfig> groups;
    MorphologyMutationParams mutation;
};

// Create a default morphology genome from config (uniform angle spacing, equal arc fracs).
MorphologyGenome create_default_morphology(const MorphologyEvolutionConfig& config);

// Mutate a morphology genome in place.
void mutate_morphology(MorphologyGenome& genome,
                       const MorphologyEvolutionConfig& config,
                       std::mt19937& rng);

// Crossover two morphology genomes. Per-eye parent selection (50/50).
MorphologyGenome crossover_morphology(const MorphologyGenome& fitter,
                                      const MorphologyGenome& other,
                                      std::mt19937& rng);

// Apply a morphology genome to a base compound eye config, producing concrete eye positions.
// The base config provides channels, proprioceptive sensors, etc. — morphology only changes
// eye angles and arc widths. Groups map by index: group 0 → eyes, group 1 → long_range_eyes.
CompoundEyeConfig apply_morphology(const CompoundEyeConfig& base,
                                   const MorphologyGenome& morpho,
                                   const MorphologyEvolutionConfig& config);

// Validate that morphology evolution config eye counts match the boid's compound eye layout.
// Returns empty string on success, or a descriptive error message on mismatch.
std::string validate_morphology_config(const CompoundEyeConfig& eyes,
                                       const MorphologyEvolutionConfig& config);
