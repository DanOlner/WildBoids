#pragma once

#include "simulation/boid.h"
#include "simulation/food_source.h"
#include "simulation/sensor.h"
#include "simulation/spatial_grid.h"
#include <vector>
#include <random>

struct Food {
    Vec2 position;
    float energy_value = 10.0f;
};

struct WorldConfig {
    float width = 1000.0f;
    float height = 1000.0f;
    bool toroidal = true;
    float linear_drag = 0.05f;
    float angular_drag = 0.1f;
    float grid_cell_size = 100.0f;

    // Food (flat fields kept for backward compat with tests)
    float food_spawn_rate = 2.0f;      // new food per second
    int food_max = 100;                 // cap on food count
    float food_eat_radius = 8.0f;      // how close to eat
    float food_energy = 10.0f;         // energy per food item

    // Food source config — if set, overrides flat fields above.
    // If left as default UniformFoodConfig, constructor syncs from flat fields.
    FoodSourceConfig food_source_config = UniformFoodConfig{};

    // Proprioceptive sensor normalization
    float max_speed = 50.0f;           // speed sensor normalization constant (speed / max_speed, clamped to [0,1])
    float max_angular_speed = 10.0f;   // angular velocity sensor normalization (angular_vel / max, clamped to [-1,1])

    // Energy costs
    float metabolism_rate = 0.5f;      // energy lost per second just being alive
    float thrust_cost = 0.1f;          // energy cost per unit thrust per second

    // Predation
    float predator_catch_radius = 12.0f;  // how close predator must be to catch prey
    float predator_catch_energy = 50.0f;  // energy gained by predator per catch

    // Directional mouth — requires boid to face and approach target to eat/catch
    bool mouth_enabled = false;
    float mouth_arc_width = 3.14159265f;  // radians (default π = 180°, front hemisphere)
    bool mouth_require_approach = true;   // velocity dot-product check

    // Shoaling: drag reduction from same-type neighbours
    struct ShoalingConfig {
        float radius = 0.0f;            // 0 = disabled
        float max_reduction = 0.0f;     // max drag reduction fraction (0.3 = 30%)
        int   max_neighbours = 6;       // count that gives max reduction
        float arc = 4.71238898f;        // forward arc in radians (default 270° = 3π/2), only neighbours within this arc count
    };
    ShoalingConfig prey_shoaling;
    ShoalingConfig predator_shoaling;

    // Sensor channel enable/disable (compound eyes only)
    // Disabled channels produce 0.0 — NEAT inputs still exist but carry no information
    std::vector<SensorChannel> enabled_channels = {
        SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite
    };
};

class World {
public:
    explicit World(const WorldConfig& config);

    void add_boid(Boid boid);
    void add_food(Food food);
    void step(float dt, std::mt19937* rng = nullptr);

    // Pre-seed food using the configured food source strategy.
    void pre_seed_food(std::mt19937& rng);

    const std::vector<Boid>& get_boids() const;
    std::vector<Boid>& get_boids_mut();
    const WorldConfig& get_config() const;
    const SpatialGrid& grid() const;
    const std::vector<Food>& get_food() const;

private:
    WorldConfig config_;
    std::vector<Boid> boids_;
    std::vector<Food> food_;
    SpatialGrid grid_;
    FoodSource food_source_;

    void wrap_position(Vec2& pos) const;
    void rebuild_grid();
    void run_sensors(std::mt19937* rng);
    void run_brains();
    void spawn_food(float dt, std::mt19937& rng);
    void check_food_eating();
    void check_predation();
    void deduct_energy(float dt);
    void compute_shoaling();
};
