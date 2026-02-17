#pragma once

#include "simulation/boid.h"
#include "simulation/spatial_grid.h"
#include <vector>

struct WorldConfig {
    float width = 1000.0f;
    float height = 1000.0f;
    bool toroidal = true;
    float linear_drag = 0.05f;
    float angular_drag = 0.1f;
    float grid_cell_size = 100.0f;
};

class World {
public:
    explicit World(const WorldConfig& config);

    void add_boid(Boid boid);
    void step(float dt);

    const std::vector<Boid>& get_boids() const;
    std::vector<Boid>& get_boids_mut();
    const WorldConfig& get_config() const;
    const SpatialGrid& grid() const;

private:
    WorldConfig config_;
    std::vector<Boid> boids_;
    SpatialGrid grid_;

    void wrap_position(Vec2& pos) const;
    void rebuild_grid();
    void run_sensors();
};
