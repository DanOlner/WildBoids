#pragma once

#include "simulation/sensor.h"
#include "simulation/vec2.h"
#include <vector>

struct Boid;
struct WorldConfig;
class SpatialGrid;

class SensorySystem {
public:
    explicit SensorySystem(std::vector<SensorSpec> specs);

    int input_count() const { return static_cast<int>(specs_.size()); }
    const std::vector<SensorSpec>& specs() const { return specs_; }

    // Fill outputs[0..input_count()-1] with sensor readings for boid at self_index.
    void perceive(const std::vector<Boid>& boids,
                  const SpatialGrid& grid,
                  const WorldConfig& config,
                  int self_index,
                  float* outputs) const;

private:
    std::vector<SensorSpec> specs_;

    float evaluate_sensor(const SensorSpec& spec,
                          const Boid& self,
                          const std::vector<Boid>& boids,
                          const SpatialGrid& grid,
                          const WorldConfig& config) const;
};
