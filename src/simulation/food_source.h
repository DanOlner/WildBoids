#pragma once

#include "simulation/vec2.h"
#include <random>
#include <variant>
#include <vector>

struct Food;  // forward declare from world.h

// --- Config types ---

struct UniformFoodConfig {
    float spawn_rate = 2.0f;   // food items per second
    int max_food = 100;        // cap on total food
    float energy = 10.0f;      // energy per food item
};

struct PatchFoodConfig {
    int num_patches = 2;         // number of active patches
    int food_per_patch = 30;     // items per patch
    float patch_radius = 50.0f;  // clustering spread (std dev)
    float energy = 10.0f;
};

using FoodSourceConfig = std::variant<UniformFoodConfig, PatchFoodConfig>;

// --- Food source types ---

class UniformFoodSource {
public:
    UniformFoodSource(UniformFoodConfig config, float world_w, float world_h);

    void spawn(std::vector<Food>& food, float dt, std::mt19937& rng);
    void pre_seed(std::vector<Food>& food, std::mt19937& rng);

private:
    UniformFoodConfig config_;
    float world_w_, world_h_;
};

class PatchFoodSource {
public:
    PatchFoodSource(PatchFoodConfig config, float world_w, float world_h);

    void spawn(std::vector<Food>& food, float dt, std::mt19937& rng);
    void pre_seed(std::vector<Food>& food, std::mt19937& rng);

    int target_count() const;

private:
    PatchFoodConfig config_;
    float world_w_, world_h_;

    void spawn_patch(std::vector<Food>& food, std::mt19937& rng);
};

using FoodSource = std::variant<UniformFoodSource, PatchFoodSource>;

// Factory: build a FoodSource from config + world dimensions.
FoodSource make_food_source(const FoodSourceConfig& config, float world_w, float world_h);
