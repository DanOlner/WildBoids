#include "simulation/sensory_system.h"
#include "simulation/boid.h"
#include "simulation/world.h"
#include "simulation/spatial_grid.h"
#include "simulation/toroidal.h"
#include <cmath>
#include <algorithm>

SensorySystem::SensorySystem(std::vector<SensorSpec> specs)
    : specs_(std::move(specs)) {}

SensorySystem::SensorySystem(CompoundEyeConfig eye_config)
    : eye_config_(std::move(eye_config)) {}

int SensorySystem::input_count() const {
    if (is_compound()) return eye_config_->total_inputs();
    return static_cast<int>(specs_.size());
}

void SensorySystem::perceive(const std::vector<Boid>& boids,
                              const SpatialGrid& grid,
                              const WorldConfig& config,
                              int self_index,
                              const std::vector<Food>& food,
                              float* outputs,
                              std::mt19937* rng) const {
    if (is_compound()) {
        perceive_compound(boids, grid, config, self_index, food, outputs, rng);
        return;
    }
    const Boid& self = boids[self_index];
    for (int i = 0; i < static_cast<int>(specs_.size()); ++i) {
        outputs[i] = evaluate_sensor(specs_[i], self, boids, grid, config, food);
    }
}

static bool passes_filter(EntityFilter filter, const std::string& type) {
    switch (filter) {
        case EntityFilter::Any:      return true;
        case EntityFilter::Prey:     return type == "prey";
        case EntityFilter::Predator: return type == "predator";
        case EntityFilter::Food:     return false; // food filter doesn't match boids
        case EntityFilter::Speed:    return false; // proprioceptive, not spatial
        case EntityFilter::AngularVelocity: return false; // proprioceptive, not spatial
        case EntityFilter::Noise: return false; // proprioceptive, not spatial
    }
    return false;
}

// Shared arc check: given a position delta from self, test against sensor arc.
// Returns true if the position is within the arc, and updates nearest_dist_sq/count.
static bool check_arc(const SensorSpec& spec, const Boid& self,
                       Vec2 delta, float range_sq,
                       float& nearest_dist_sq, int& count) {
    float dist_sq = delta.length_squared();
    if (dist_sq > range_sq) return false;

    Vec2 body_delta = delta.rotated(-self.body.angle);
    float angle = std::atan2(body_delta.x, body_delta.y);
    if (!angle_in_arc(angle, spec.center_angle, spec.arc_width)) return false;

    count++;
    if (dist_sq < nearest_dist_sq) {
        nearest_dist_sq = dist_sq;
    }
    return true;
}

static float compute_signal(SignalType signal_type, float nearest_dist_sq,
                             float range_sq, float max_range, int count) {
    switch (signal_type) {
        case SignalType::NearestDistance: {
            if (nearest_dist_sq > range_sq) return 0.0f;
            float dist = std::sqrt(nearest_dist_sq);
            return 1.0f - (dist / max_range);
        }
        case SignalType::SectorDensity: {
            constexpr float expected_max = 10.0f;
            return std::min(1.0f, static_cast<float>(count) / expected_max);
        }
    }
    return 0.0f;
}

float SensorySystem::evaluate_sensor(const SensorSpec& spec,
                                      const Boid& self,
                                      const std::vector<Boid>& boids,
                                      const SpatialGrid& grid,
                                      const WorldConfig& config,
                                      const std::vector<Food>& food) const {
    // Proprioceptive sensors — read internal state, no spatial query
    if (spec.filter == EntityFilter::Speed) {
        float speed = self.body.velocity.length();
        float max_speed = config.max_speed;
        if (max_speed <= 0.0f) return 0.0f;
        return std::min(1.0f, speed / max_speed);
    }
    if (spec.filter == EntityFilter::AngularVelocity) {
        float max_av = config.max_angular_speed;
        if (max_av <= 0.0f) return 0.0f;
        return std::max(-1.0f, std::min(1.0f, self.body.angular_velocity / max_av));
    }
    if (spec.filter == EntityFilter::Noise) {
        return 0.0f; // legacy path has no RNG access
    }

    float range_sq = spec.max_range * spec.max_range;
    float nearest_dist_sq = range_sq + 1.0f;
    int count = 0;

    if (spec.filter == EntityFilter::Food) {
        // Brute-force iterate food (Option A — food vector is small)
        for (const auto& f : food) {
            Vec2 delta = toroidal_delta(self.body.position, f.position,
                                         config.width, config.height);
            check_arc(spec, self, delta, range_sq, nearest_dist_sq, count);
        }
    } else {
        // Use spatial grid for boid detection
        std::vector<int> candidates;
        grid.query(self.body.position, spec.max_range, candidates);

        for (int j : candidates) {
            if (&boids[j] == &self) continue;
            if (!passes_filter(spec.filter, boids[j].type)) continue;

            Vec2 delta = toroidal_delta(self.body.position, boids[j].body.position,
                                         config.width, config.height);
            check_arc(spec, self, delta, range_sq, nearest_dist_sq, count);
        }
    }

    return compute_signal(spec.signal_type, nearest_dist_sq, range_sq,
                           spec.max_range, count);
}

// Check if a channel is enabled in the world config
static bool channel_enabled(SensorChannel ch, const std::vector<SensorChannel>& enabled) {
    for (auto e : enabled) {
        if (e == ch) return true;
    }
    return false;
}

void SensorySystem::perceive_compound(const std::vector<Boid>& boids,
                                       const SpatialGrid& grid,
                                       const WorldConfig& config,
                                       int self_index,
                                       const std::vector<Food>& food,
                                       float* outputs,
                                       std::mt19937* rng) const {
    const Boid& self = boids[self_index];
    const auto& cfg = *eye_config_;
    int n_channels = static_cast<int>(cfg.channels.size());
    int n_short_eyes = cfg.short_range_eye_count();
    int n_long_eyes = cfg.long_range_eye_count();
    int total = cfg.total_inputs();

    // Output layout: [short eyes × channels, long eyes × channels, proprioceptive]
    int long_range_offset = n_short_eyes * n_channels;

    // Zero all outputs
    for (int i = 0; i < total; ++i) outputs[i] = 0.0f;

    // Find channel indices for each type
    int food_ch = -1, same_ch = -1, opposite_ch = -1;
    for (int c = 0; c < n_channels; ++c) {
        if (cfg.channels[c] == SensorChannel::Food) food_ch = c;
        else if (cfg.channels[c] == SensorChannel::Same) same_ch = c;
        else if (cfg.channels[c] == SensorChannel::Opposite) opposite_ch = c;
    }

    // Check which channels are enabled in world config
    bool food_enabled = food_ch >= 0 && channel_enabled(SensorChannel::Food, config.enabled_channels);
    bool same_enabled = same_ch >= 0 && channel_enabled(SensorChannel::Same, config.enabled_channels);
    bool opposite_enabled = opposite_ch >= 0 && channel_enabled(SensorChannel::Opposite, config.enabled_channels);

    // Helper: check a target against all eyes in a list, updating outputs
    auto process_eyes = [&](const std::vector<EyeSpec>& eye_list, int out_offset,
                            float angle, float dist_sq, int ch_idx) {
        for (int e = 0; e < static_cast<int>(eye_list.size()); ++e) {
            const auto& eye = eye_list[e];
            float range_sq = eye.max_range * eye.max_range;
            if (dist_sq > range_sq) continue;
            if (!angle_in_arc(angle, eye.center_angle, eye.arc_width)) continue;

            int out_idx = out_offset + e * n_channels + ch_idx;
            float dist = std::sqrt(dist_sq);
            float signal = 1.0f - (dist / eye.max_range);
            if (signal > outputs[out_idx]) {
                outputs[out_idx] = signal;
            }
        }
    };

    // --- Boid channels (Same, Opposite) ---
    if (same_enabled || opposite_enabled) {
        // Find max range across all eyes (both tiers) for a single grid query
        float max_range = 0;
        for (const auto& eye : cfg.eyes)
            max_range = std::max(max_range, eye.max_range);
        for (const auto& eye : cfg.long_range_eyes)
            max_range = std::max(max_range, eye.max_range);

        std::vector<int> candidates;
        grid.query(self.body.position, max_range, candidates);

        for (int j : candidates) {
            if (&boids[j] == &self) continue;
            if (!boids[j].alive) continue;

            bool is_same_type = (boids[j].type == self.type);
            int ch_idx;
            if (is_same_type) {
                if (!same_enabled) continue;
                ch_idx = same_ch;
            } else {
                if (!opposite_enabled) continue;
                ch_idx = opposite_ch;
            }

            Vec2 delta = toroidal_delta(self.body.position, boids[j].body.position,
                                         config.width, config.height);

            // Rotate to body frame once per candidate
            Vec2 body_delta = delta.rotated(-self.body.angle);
            float angle = std::atan2(body_delta.x, body_delta.y);
            float dist_sq = delta.length_squared();

            process_eyes(cfg.eyes, 0, angle, dist_sq, ch_idx);
            process_eyes(cfg.long_range_eyes, long_range_offset, angle, dist_sq, ch_idx);
        }
    }

    // --- Food channel ---
    if (food_enabled) {
        for (const auto& f : food) {
            Vec2 delta = toroidal_delta(self.body.position, f.position,
                                         config.width, config.height);

            Vec2 body_delta = delta.rotated(-self.body.angle);
            float angle = std::atan2(body_delta.x, body_delta.y);
            float dist_sq = delta.length_squared();

            process_eyes(cfg.eyes, 0, angle, dist_sq, food_ch);
            process_eyes(cfg.long_range_eyes, long_range_offset, angle, dist_sq, food_ch);
        }
    }

    // --- Proprioceptive sensors (appended after both eye tiers) ---
    int proprio_idx = (n_short_eyes + n_long_eyes) * n_channels;
    if (cfg.has_speed_sensor) {
        float speed = self.body.velocity.length();
        float max_speed = config.max_speed;
        outputs[proprio_idx] = (max_speed > 0) ? std::min(1.0f, speed / max_speed) : 0.0f;
        proprio_idx++;
    }
    if (cfg.has_angular_velocity_sensor) {
        float max_av = config.max_angular_speed;
        outputs[proprio_idx] = (max_av > 0)
            ? std::max(-1.0f, std::min(1.0f, self.body.angular_velocity / max_av))
            : 0.0f;
        proprio_idx++;
    }
    if (cfg.has_noise_sensor) {
        if (rng) {
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            outputs[proprio_idx] = dist(*rng);
        } else {
            outputs[proprio_idx] = 0.0f;
        }
        proprio_idx++;
    }
    if (cfg.has_shoaling_sensor) {
        float base_drag = config.linear_drag;
        float bonus = (base_drag > 0.0f && self.effective_linear_drag >= 0.0f)
                      ? (1.0f - self.effective_linear_drag / base_drag)
                      : 0.0f;
        outputs[proprio_idx] = std::clamp(bonus, 0.0f, 1.0f);
        proprio_idx++;
    }
    if (cfg.has_hunger_sensor) {
        float hunger = (self.initial_energy > 0.0f)
            ? 1.0f - std::clamp(self.energy / self.initial_energy, 0.0f, 1.0f)
            : 0.0f;
        outputs[proprio_idx] = hunger;
        proprio_idx++;
    }
}
