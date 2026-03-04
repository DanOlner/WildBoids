#include "simulation/morphology_genome.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>

static constexpr float PI = static_cast<float>(M_PI);
static constexpr float DEG_TO_RAD = PI / 180.0f;

static float wrap_angle(float a) {
    a = std::fmod(a + PI, 2.0f * PI);
    if (a < 0) a += 2.0f * PI;
    return a - PI;
}

MorphologyGenome create_default_morphology(const MorphologyEvolutionConfig& config) {
    MorphologyGenome genome;
    genome.groups.reserve(config.groups.size());

    for (const auto& gc : config.groups) {
        SensorGroupMorphology group;
        group.angles.resize(gc.eye_count);
        group.arc_fracs.resize(gc.eye_count, 1.0f);  // equal fractions

        // Evenly space eyes around the circle
        float step = 2.0f * PI / gc.eye_count;
        for (int i = 0; i < gc.eye_count; ++i) {
            group.angles[i] = wrap_angle(i * step);
        }

        genome.groups.push_back(std::move(group));
    }

    return genome;
}

void mutate_morphology(MorphologyGenome& genome,
                       const MorphologyEvolutionConfig& config,
                       std::mt19937& rng) {
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);

    for (size_t gi = 0; gi < genome.groups.size() && gi < config.groups.size(); ++gi) {
        auto& group = genome.groups[gi];
        float angle_sigma = config.mutation.angle_sigma_deg * DEG_TO_RAD;

        for (size_t i = 0; i < group.angles.size(); ++i) {
            // Angle mutation
            if (coin(rng) < config.mutation.angle_mutate_prob) {
                if (coin(rng) < config.mutation.replace_prob) {
                    // Full replacement: random angle
                    std::uniform_real_distribution<float> angle_dist(-PI, PI);
                    group.angles[i] = angle_dist(rng);
                } else {
                    // Gaussian perturbation
                    std::normal_distribution<float> perturb(0.0f, angle_sigma);
                    group.angles[i] = wrap_angle(group.angles[i] + perturb(rng));
                }
            }

            // Arc fraction mutation
            if (coin(rng) < config.mutation.arc_mutate_prob) {
                if (coin(rng) < config.mutation.replace_prob) {
                    // Full replacement: random fraction
                    std::uniform_real_distribution<float> frac_dist(
                        config.mutation.min_arc_frac, 3.0f);
                    group.arc_fracs[i] = frac_dist(rng);
                } else {
                    // Gaussian perturbation
                    std::normal_distribution<float> perturb(0.0f, config.mutation.arc_frac_sigma);
                    group.arc_fracs[i] += perturb(rng);
                }
                // Clamp to minimum
                group.arc_fracs[i] = std::max(config.mutation.min_arc_frac, group.arc_fracs[i]);
            }
        }
    }
}

MorphologyGenome crossover_morphology(const MorphologyGenome& fitter,
                                      const MorphologyGenome& other,
                                      std::mt19937& rng) {
    MorphologyGenome child;
    child.groups.resize(fitter.groups.size());

    std::uniform_real_distribution<float> coin(0.0f, 1.0f);

    for (size_t gi = 0; gi < fitter.groups.size(); ++gi) {
        const auto& fg = fitter.groups[gi];
        const auto& og = (gi < other.groups.size()) ? other.groups[gi] : fg;
        auto& cg = child.groups[gi];

        cg.angles.resize(fg.angles.size());
        cg.arc_fracs.resize(fg.arc_fracs.size());

        for (size_t i = 0; i < fg.angles.size(); ++i) {
            if (i < og.angles.size() && coin(rng) < 0.5f) {
                cg.angles[i] = og.angles[i];
                cg.arc_fracs[i] = og.arc_fracs[i];
            } else {
                cg.angles[i] = fg.angles[i];
                cg.arc_fracs[i] = fg.arc_fracs[i];
            }
        }
    }

    return child;
}

// Apply morphology genome to a base config, producing concrete eye positions.
// Group 0 maps to short-range eyes, group 1 to long-range eyes.
static void apply_group(std::vector<EyeSpec>& eyes,
                        const SensorGroupMorphology& morpho,
                        const MorphologyGroupConfig& gc) {
    float budget = gc.total_arc_deg * DEG_TO_RAD;
    float total_frac = std::accumulate(morpho.arc_fracs.begin(), morpho.arc_fracs.end(), 0.0f);
    if (total_frac <= 0.0f) total_frac = 1.0f;

    int count = std::min(static_cast<int>(eyes.size()),
                         static_cast<int>(morpho.angles.size()));
    for (int i = 0; i < count; ++i) {
        eyes[i].center_angle = morpho.angles[i];
        eyes[i].arc_width = (morpho.arc_fracs[i] / total_frac) * budget;
        eyes[i].max_range = gc.max_range;
    }
}

CompoundEyeConfig apply_morphology(const CompoundEyeConfig& base,
                                   const MorphologyGenome& morpho,
                                   const MorphologyEvolutionConfig& config) {
    CompoundEyeConfig result = base;

    // Group 0 → short-range eyes
    if (morpho.groups.size() > 0 && config.groups.size() > 0) {
        apply_group(result.eyes, morpho.groups[0], config.groups[0]);
    }

    // Group 1 → long-range eyes
    if (morpho.groups.size() > 1 && config.groups.size() > 1) {
        apply_group(result.long_range_eyes, morpho.groups[1], config.groups[1]);
    }

    // Additional groups beyond 2 would need CompoundEyeConfig to support more tiers.
    // For now, groups 0 and 1 are the only ones applied.

    return result;
}

std::string validate_morphology_config(const CompoundEyeConfig& eyes,
                                       const MorphologyEvolutionConfig& config) {
    std::ostringstream err;

    if (config.groups.size() >= 1) {
        int spec_short = static_cast<int>(eyes.eyes.size());
        int cfg_short = config.groups[0].eye_count;
        if (spec_short != cfg_short) {
            err << "Morphology group 0 (short-range) eye count mismatch: "
                << "boid spec has " << spec_short << " eyes, "
                << "morphologyEvolution config has " << cfg_short;
        }
    }

    if (config.groups.size() >= 2) {
        int spec_long = static_cast<int>(eyes.long_range_eyes.size());
        int cfg_long = config.groups[1].eye_count;
        if (spec_long != cfg_long) {
            if (!err.str().empty()) err << "; ";
            err << "Morphology group 1 (long-range) eye count mismatch: "
                << "boid spec has " << spec_long << " eyes, "
                << "morphologyEvolution config has " << cfg_long;
        }
    }

    return err.str();
}
