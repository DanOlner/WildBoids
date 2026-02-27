#pragma once

#include "simulation/sensor.h"
#include "simulation/vec2.h"
#include <vector>
#include <optional>
#include <random>

struct Boid;
struct Food;
struct WorldConfig;
class SpatialGrid;

class SensorySystem {
public:
    // Legacy constructor (old-style flat sensor list)
    explicit SensorySystem(std::vector<SensorSpec> specs);

    // Compound-eye constructor
    explicit SensorySystem(CompoundEyeConfig eye_config);

    int input_count() const;
    const std::vector<SensorSpec>& specs() const { return specs_; }

    bool is_compound() const { return eye_config_.has_value(); }
    const CompoundEyeConfig& compound_config() const { return *eye_config_; }

    // Fill outputs[0..input_count()-1] with sensor readings for boid at self_index.
    void perceive(const std::vector<Boid>& boids,
                  const SpatialGrid& grid,
                  const WorldConfig& config,
                  int self_index,
                  const std::vector<Food>& food,
                  float* outputs,
                  std::mt19937* rng = nullptr) const;

private:
    std::vector<SensorSpec> specs_;                 // legacy mode
    std::optional<CompoundEyeConfig> eye_config_;   // compound-eye mode

    // Legacy path
    float evaluate_sensor(const SensorSpec& spec,
                          const Boid& self,
                          const std::vector<Boid>& boids,
                          const SpatialGrid& grid,
                          const WorldConfig& config,
                          const std::vector<Food>& food) const;

    // Compound-eye path
    void perceive_compound(const std::vector<Boid>& boids,
                           const SpatialGrid& grid,
                           const WorldConfig& config,
                           int self_index,
                           const std::vector<Food>& food,
                           float* outputs,
                           std::mt19937* rng) const;
};
