#include "simulation/world.h"
#include <cmath>

World::World(const WorldConfig& config) : config_(config) {}

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

void World::wrap_position(Vec2& pos) const {
    pos.x = std::fmod(pos.x, config_.width);
    if (pos.x < 0) pos.x += config_.width;

    pos.y = std::fmod(pos.y, config_.height);
    if (pos.y < 0) pos.y += config_.height;
}
