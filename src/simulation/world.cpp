#include "simulation/world.h"
#include "simulation/sensor.h"
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
        float drag = (boid.effective_linear_drag >= 0.0f)
                     ? boid.effective_linear_drag
                     : config_.linear_drag;
        boid.step(dt, drag, config_.angular_drag);
        if (config_.toroidal) {
            wrap_position(boid.body.position);
        }
    }

    rebuild_grid();
    compute_shoaling();
    run_sensors(rng);
    run_brains();
    deduct_energy(dt);
    check_food_eating();
    check_predation();

    if (rng) {
        spawn_food(dt, *rng);
    }
}

void World::pre_seed_food(std::mt19937& rng) {
    std::visit([&](auto& source) {
        source.pre_seed(food_, rng);
    }, food_source_);
}

void World::run_sensors(std::mt19937* rng) {
    for (int i = 0; i < static_cast<int>(boids_.size()); ++i) {
        auto& boid = boids_[i];
        if (!boid.alive) continue;
        if (!boid.sensors) continue;
        boid.sensor_outputs.resize(boid.sensors->input_count());
        boid.sensors->perceive(boids_, grid_, config_, i, food_,
                               boid.sensor_outputs.data(), rng);
    }
}

void World::refresh_sensors(int boid_index, std::mt19937* rng) {
    rebuild_grid();
    if (boid_index >= 0 && boid_index < static_cast<int>(boids_.size())) {
        auto& boid = boids_[boid_index];
        if (boid.alive && boid.sensors) {
            boid.sensor_outputs.resize(boid.sensors->input_count());
            boid.sensors->perceive(boids_, grid_, config_, boid_index, food_,
                                   boid.sensor_outputs.data(), rng);
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

void World::compute_shoaling() {
    // Early exit if both types disabled (radius 0)
    if (config_.prey_shoaling.radius <= 0.0f &&
        config_.predator_shoaling.radius <= 0.0f) {
        // Reset all to world default
        for (auto& boid : boids_) {
            boid.effective_linear_drag = config_.linear_drag;
        }
        return;
    }

    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(boids_.size()); ++i) {
        auto& boid = boids_[i];
        if (!boid.alive) {
            boid.effective_linear_drag = config_.linear_drag;
            continue;
        }

        const auto& shoal_cfg = (boid.type == "predator")
                                ? config_.predator_shoaling
                                : config_.prey_shoaling;

        if (shoal_cfg.radius <= 0.0f) {
            boid.effective_linear_drag = config_.linear_drag;
            continue;
        }

        // Query grid for nearby boids
        candidates.clear();
        grid_.query(boid.body.position, shoal_cfg.radius, candidates);

        float radius_sq = shoal_cfg.radius * shoal_cfg.radius;
        int count = 0;
        for (int j : candidates) {
            if (j == i) continue;
            if (!boids_[j].alive) continue;
            if (boids_[j].type != boid.type) continue;

            Vec2 delta;
            if (config_.toroidal) {
                delta = toroidal_delta(boid.body.position, boids_[j].body.position,
                                       config_.width, config_.height);
            } else {
                delta = boids_[j].body.position - boid.body.position;
            }
            if (delta.length_squared() > radius_sq) continue;

            // Arc check: only count neighbours within the forward arc
            if (shoal_cfg.arc < 6.28f) {  // skip if full circle (2π)
                Vec2 body_delta = delta.rotated(-boid.body.angle);
                float angle = std::atan2(body_delta.x, body_delta.y);
                if (!angle_in_arc(angle, 0.0f, shoal_cfg.arc)) continue;
            }
            ++count;
        }

        float fraction = static_cast<float>(std::min(count, shoal_cfg.max_neighbours))
                       / static_cast<float>(shoal_cfg.max_neighbours);
        boid.effective_linear_drag = config_.linear_drag * (1.0f - shoal_cfg.max_reduction * fraction);
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

void World::check_predation() {
    float catch_radius_sq = config_.predator_catch_radius * config_.predator_catch_radius;

    for (auto& predator : boids_) {
        if (!predator.alive) continue;
        if (predator.type != "predator") continue;

        for (auto& prey : boids_) {
            if (!prey.alive) continue;
            if (prey.type != "prey") continue;

            Vec2 delta;
            if (config_.toroidal) {
                delta = toroidal_delta(predator.body.position, prey.body.position,
                                       config_.width, config_.height);
            } else {
                delta = prey.body.position - predator.body.position;
            }
            float dist_sq = delta.length_squared();

            if (dist_sq > catch_radius_sq) continue;

            if (config_.mouth_enabled) {
                Vec2 body_delta = delta.rotated(-predator.body.angle);
                float angle = std::atan2(body_delta.x, body_delta.y);
                if (!angle_in_arc(angle, 0.0f, config_.mouth_arc_width))
                    continue;
                if (config_.mouth_require_approach && delta.dot(predator.body.velocity) <= 0.0f)
                    continue;
            }

            // Prey dies
            prey.alive = false;
            prey.energy = 0.0f;
            for (auto& t : prey.thrusters) t.power = 0.0f;

            // Predator gains energy
            predator.energy += config_.predator_catch_energy;
            predator.total_energy_gained += config_.predator_catch_energy;
        }
    }
}

void World::check_food_eating() {
    float eat_radius_sq = config_.food_eat_radius * config_.food_eat_radius;

    for (auto& boid : boids_) {
        if (!boid.alive) continue;
        if (boid.type == "predator") continue;  // predators don't eat food

        food_.erase(
            std::remove_if(food_.begin(), food_.end(),
                [&](const Food& f) {
                    Vec2 delta;
                    if (config_.toroidal) {
                        delta = toroidal_delta(boid.body.position, f.position,
                                               config_.width, config_.height);
                    } else {
                        delta = f.position - boid.body.position;
                    }
                    float dist_sq = delta.length_squared();

                    if (dist_sq > eat_radius_sq) return false;

                    if (config_.mouth_enabled) {
                        Vec2 body_delta = delta.rotated(-boid.body.angle);
                        float angle = std::atan2(body_delta.x, body_delta.y);
                        if (!angle_in_arc(angle, 0.0f, config_.mouth_arc_width))
                            return false;
                        if (config_.mouth_require_approach && delta.dot(boid.body.velocity) <= 0.0f)
                            return false;
                    }

                    boid.energy += f.energy_value;
                    boid.total_energy_gained += f.energy_value;
                    return true;  // remove this food
                }),
            food_.end());
    }
}

void World::deduct_energy(float dt) {
    for (auto& boid : boids_) {
        if (!boid.alive) continue;

        // Metabolism cost
        float metabolism = config_.metabolism_rate * dt;
        boid.energy -= metabolism;
        boid.total_energy_spent += metabolism;

        // Thrust cost
        float thrust_cost = config_.thrust_cost * boid.total_thrust() * dt;
        boid.energy -= thrust_cost;
        boid.total_energy_spent += thrust_cost;

        // Death
        if (boid.energy <= 0.0f) {
            boid.energy = 0.0f;
            boid.alive = false;
            // Zero all thrusters
            for (auto& t : boid.thrusters) t.power = 0.0f;
        }
    }
}
