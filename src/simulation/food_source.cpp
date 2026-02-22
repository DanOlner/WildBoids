#include "simulation/food_source.h"
#include "simulation/world.h"  // for Food struct
#include <cmath>

// --- UniformFoodSource ---

UniformFoodSource::UniformFoodSource(UniformFoodConfig config, float world_w, float world_h)
    : config_(config), world_w_(world_w), world_h_(world_h) {}

void UniformFoodSource::spawn(std::vector<Food>& food, float dt, std::mt19937& rng) {
    if (static_cast<int>(food.size()) >= config_.max_food) return;

    float expected = config_.spawn_rate * dt;
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    int to_spawn = static_cast<int>(expected);
    float fractional = expected - static_cast<float>(to_spawn);
    if (prob(rng) < fractional) ++to_spawn;

    std::uniform_real_distribution<float> x_dist(0.0f, world_w_);
    std::uniform_real_distribution<float> y_dist(0.0f, world_h_);

    for (int i = 0; i < to_spawn; ++i) {
        if (static_cast<int>(food.size()) >= config_.max_food) break;
        food.push_back(Food{Vec2{x_dist(rng), y_dist(rng)}, config_.energy});
    }
}

void UniformFoodSource::pre_seed(std::vector<Food>& food, std::mt19937& rng) {
    std::uniform_real_distribution<float> x_dist(0.0f, world_w_);
    std::uniform_real_distribution<float> y_dist(0.0f, world_h_);

    int count = config_.max_food / 2;
    for (int i = 0; i < count; ++i) {
        food.push_back(Food{Vec2{x_dist(rng), y_dist(rng)}, config_.energy});
    }
}

// --- PatchFoodSource ---

PatchFoodSource::PatchFoodSource(PatchFoodConfig config, float world_w, float world_h)
    : config_(config), world_w_(world_w), world_h_(world_h) {}

int PatchFoodSource::target_count() const {
    return config_.num_patches * config_.food_per_patch;
}

void PatchFoodSource::spawn(std::vector<Food>& food, float dt, std::mt19937& rng) {
    // When enough food has been eaten (a whole patch worth), spawn a new patch.
    int target = target_count();
    while (static_cast<int>(food.size()) + config_.food_per_patch <= target) {
        spawn_patch(food, rng);
    }
}

void PatchFoodSource::pre_seed(std::vector<Food>& food, std::mt19937& rng) {
    for (int i = 0; i < config_.num_patches; ++i) {
        spawn_patch(food, rng);
    }
}

void PatchFoodSource::spawn_patch(std::vector<Food>& food, std::mt19937& rng) {
    std::uniform_real_distribution<float> x_dist(0.0f, world_w_);
    std::uniform_real_distribution<float> y_dist(0.0f, world_h_);
    Vec2 center{x_dist(rng), y_dist(rng)};

    std::normal_distribution<float> offset(0.0f, config_.patch_radius);

    for (int i = 0; i < config_.food_per_patch; ++i) {
        float px = center.x + offset(rng);
        float py = center.y + offset(rng);
        // Wrap to world bounds (handles toroidal + out-of-range positions)
        px = std::fmod(px + world_w_, world_w_);
        py = std::fmod(py + world_h_, world_h_);
        food.push_back(Food{Vec2{px, py}, config_.energy});
    }
}

// --- Factory ---

FoodSource make_food_source(const FoodSourceConfig& config, float world_w, float world_h) {
    return std::visit([&](const auto& cfg) -> FoodSource {
        using T = std::decay_t<decltype(cfg)>;
        if constexpr (std::is_same_v<T, UniformFoodConfig>) {
            return UniformFoodSource(cfg, world_w, world_h);
        } else {
            return PatchFoodSource(cfg, world_w, world_h);
        }
    }, config);
}
