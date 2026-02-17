#include "simulation/world.h"
#include <cmath>

World::World(const WorldConfig& config)
    : config_(config)
    , grid_(config.width, config.height, config.grid_cell_size, config.toroidal)
{}

void World::add_boid(Boid boid) {
    boids_.push_back(std::move(boid));
}

void World::step(float dt) {
    for (auto& boid : boids_) {
        boid.step(dt, config_.linear_drag, config_.angular_drag);
        if (config_.toroidal) {
            wrap_position(boid.body.position);
        }
    }

    rebuild_grid();
    run_sensors();
}

void World::run_sensors() {
    for (int i = 0; i < static_cast<int>(boids_.size()); ++i) {
        auto& boid = boids_[i];
        if (!boid.sensors) continue;
        boid.sensor_outputs.resize(boid.sensors->input_count());
        boid.sensors->perceive(boids_, grid_, config_, i,
                               boid.sensor_outputs.data());
    }
}

const std::vector<Boid>& World::get_boids() const {
    return boids_;
}

std::vector<Boid>& World::get_boids_mut() {
    return boids_;
}

const WorldConfig& World::get_config() const {
    return config_;
}

const SpatialGrid& World::grid() const {
    return grid_;
}

void World::wrap_position(Vec2& pos) const {
    pos.x = std::fmod(pos.x, config_.width);
    if (pos.x < 0) pos.x += config_.width;

    pos.y = std::fmod(pos.y, config_.height);
    if (pos.y < 0) pos.y += config_.height;
}

void World::rebuild_grid() {
    grid_.clear();
    for (int i = 0; i < static_cast<int>(boids_.size()); ++i) {
        grid_.insert(i, boids_[i].body.position);
    }
}
