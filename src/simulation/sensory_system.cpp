#include "simulation/sensory_system.h"
#include "simulation/boid.h"
#include "simulation/world.h"
#include "simulation/spatial_grid.h"
#include "simulation/toroidal.h"
#include <cmath>
#include <algorithm>

SensorySystem::SensorySystem(std::vector<SensorSpec> specs)
    : specs_(std::move(specs)) {}

void SensorySystem::perceive(const std::vector<Boid>& boids,
                              const SpatialGrid& grid,
                              const WorldConfig& config,
                              int self_index,
                              const std::vector<Food>& food,
                              float* outputs) const {
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
