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
                              float* outputs) const {
    const Boid& self = boids[self_index];
    for (int i = 0; i < static_cast<int>(specs_.size()); ++i) {
        outputs[i] = evaluate_sensor(specs_[i], self, boids, grid, config);
    }
}

static bool passes_filter(EntityFilter filter, const std::string& type) {
    switch (filter) {
        case EntityFilter::Any:      return true;
        case EntityFilter::Prey:     return type == "prey";
        case EntityFilter::Predator: return type == "predator";
    }
    return false;
}

float SensorySystem::evaluate_sensor(const SensorSpec& spec,
                                      const Boid& self,
                                      const std::vector<Boid>& boids,
                                      const SpatialGrid& grid,
                                      const WorldConfig& config) const {
    std::vector<int> candidates;
    grid.query(self.body.position, spec.max_range, candidates);

    float range_sq = spec.max_range * spec.max_range;
    float nearest_dist_sq = range_sq + 1.0f; // sentinel: nothing found
    int count = 0;

    for (int j : candidates) {
        // Skip self
        if (&boids[j] == &self) continue;

        // Entity filter
        if (!passes_filter(spec.filter, boids[j].type)) continue;

        // Distance check (toroidal)
        Vec2 delta = toroidal_delta(self.body.position, boids[j].body.position,
                                     config.width, config.height);
        float dist_sq = delta.length_squared();
        if (dist_sq > range_sq) continue;

        // Rotate delta into body frame (+Y = forward)
        Vec2 body_delta = delta.rotated(-self.body.angle);

        // Angle in body frame: atan2(x, y) because +Y is forward
        float angle = std::atan2(body_delta.x, body_delta.y);

        // Arc check
        if (!angle_in_arc(angle, spec.center_angle, spec.arc_width)) continue;

        // This boid is detected
        count++;
        if (dist_sq < nearest_dist_sq) {
            nearest_dist_sq = dist_sq;
        }
    }

    switch (spec.signal_type) {
        case SignalType::NearestDistance: {
            if (nearest_dist_sq > range_sq) return 0.0f; // nothing detected
            float dist = std::sqrt(nearest_dist_sq);
            return 1.0f - (dist / spec.max_range); // 1.0 = touching, 0.0 = at max range
        }
        case SignalType::SectorDensity: {
            // Normalise by a reasonable expected max (e.g. 10 boids in one sector)
            constexpr float expected_max = 10.0f;
            return std::min(1.0f, static_cast<float>(count) / expected_max);
        }
    }
    return 0.0f;
}
