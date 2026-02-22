#include "simulation/world.h"
#include "simulation/toroidal.h"
#include <cmath>
#include <algorithm>

World::World(const WorldConfig& config)
    : config_(config)
    , grid_(config.width, config.height, config.grid_cell_size, config.toroidal)
    , food_source_(UniformFoodSource(UniformFoodConfig{}, 0, 0))  // placeholder
{
    // If food_source_config is the default UniformFoodConfig, sync from flat fields.
    // This preserves backward compat for tests that set config.food_spawn_rate etc.
    if (auto* uc = std::get_if<UniformFoodConfig>(&config_.food_source_config)) {
        uc->spawn_rate = config_.food_spawn_rate;
        uc->max_food = config_.food_max;
        uc->energy = config_.food_energy;
    }
    food_source_ = make_food_source(config_.food_source_config, config_.width, config_.height);
}

void World::add_boid(Boid boid) {
    boids_.push_back(std::move(boid));
}

void World::add_food(Food food) {
    food_.push_back(food);
}

void World::step(float dt, std::mt19937* rng) {
    for (auto& boid : boids_) {
        boid.step(dt, config_.linear_drag, config_.angular_drag);
        if (config_.toroidal) {
            wrap_position(boid.body.position);
        }
    }

    rebuild_grid();
    run_sensors();
    run_brains();
    deduct_energy(dt);
    check_food_eating();

    if (rng) {
        spawn_food(dt, *rng);
    }
}

void World::pre_seed_food(std::mt19937& rng) {
    std::visit([&](auto& source) {
        source.pre_seed(food_, rng);
    }, food_source_);
}

void World::run_sensors() {
    for (int i = 0; i < static_cast<int>(boids_.size()); ++i) {
        auto& boid = boids_[i];
        if (!boid.alive) continue;
        if (!boid.sensors) continue;
        boid.sensor_outputs.resize(boid.sensors->input_count());
        boid.sensors->perceive(boids_, grid_, config_, i, food_,
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

void World::run_brains() {
    for (auto& boid : boids_) {
        if (!boid.alive) continue;
        if (!boid.brain) continue;

        int n_in = static_cast<int>(boid.sensor_outputs.size());
        int n_out = static_cast<int>(boid.thrusters.size());

        // Allocate thruster commands buffer
        std::vector<float> commands(n_out);
        boid.brain->activate(boid.sensor_outputs.data(), n_in,
                             commands.data(), n_out);

        // Map network outputs [0,1] directly to thruster power [0,1]
        for (int i = 0; i < n_out; ++i) {
            boid.thrusters[i].power = commands[i];
        }
    }
}

void World::rebuild_grid() {
    grid_.clear();
    for (int i = 0; i < static_cast<int>(boids_.size()); ++i) {
        if (boids_[i].alive) {
            grid_.insert(i, boids_[i].body.position);
        }
    }
}

const std::vector<Food>& World::get_food() const {
    return food_;
}

void World::spawn_food(float dt, std::mt19937& rng) {
    std::visit([&](auto& source) {
        source.spawn(food_, dt, rng);
    }, food_source_);
}

void World::check_food_eating() {
    float eat_radius_sq = config_.food_eat_radius * config_.food_eat_radius;

    for (auto& boid : boids_) {
        if (!boid.alive) continue;

        food_.erase(
            std::remove_if(food_.begin(), food_.end(),
                [&](const Food& f) {
                    float dist_sq;
                    if (config_.toroidal) {
                        dist_sq = toroidal_distance_sq(
                            boid.body.position, f.position,
                            config_.width, config_.height);
                    } else {
                        Vec2 d = f.position - boid.body.position;
                        dist_sq = d.length_squared();
                    }
                    if (dist_sq <= eat_radius_sq) {
                        boid.energy += f.energy_value;
                        boid.total_energy_gained += f.energy_value;
                        return true;  // remove this food
                    }
                    return false;
                }),
            food_.end());
    }
}

void World::deduct_energy(float dt) {
    for (auto& boid : boids_) {
        if (!boid.alive) continue;

        // Metabolism cost
        boid.energy -= config_.metabolism_rate * dt;

        // Thrust cost
        boid.energy -= config_.thrust_cost * boid.total_thrust() * dt;

        // Death
        if (boid.energy <= 0.0f) {
            boid.energy = 0.0f;
            boid.alive = false;
            // Zero all thrusters
            for (auto& t : boid.thrusters) t.power = 0.0f;
        }
    }
}
