#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/food_source.h"
#include "simulation/world.h"
#include <cmath>
#include <numeric>

using Catch::Matchers::WithinAbs;

// --- UniformFoodSource ---

TEST_CASE("UniformFoodSource: pre_seed spawns half of max", "[food_source]") {
    UniformFoodConfig cfg{10.0f, 100, 15.0f};
    UniformFoodSource source(cfg, 800, 800);

    std::vector<Food> food;
    std::mt19937 rng(42);
    source.pre_seed(food, rng);

    CHECK(food.size() == 50);  // max_food / 2
    for (const auto& f : food) {
        CHECK(f.energy_value == 15.0f);
        CHECK(f.position.x >= 0.0f);
        CHECK(f.position.x < 800.0f);
    }
}

TEST_CASE("UniformFoodSource: spawn respects max", "[food_source]") {
    UniformFoodConfig cfg{10000.0f, 20, 10.0f};  // very high rate
    UniformFoodSource source(cfg, 800, 800);

    std::vector<Food> food;
    std::mt19937 rng(42);

    source.spawn(food, 1.0f, rng);
    CHECK(static_cast<int>(food.size()) <= 20);
}

// --- PatchFoodSource ---

TEST_CASE("PatchFoodSource: pre_seed spawns correct count", "[food_source]") {
    PatchFoodConfig cfg{2, 25, 40.0f, 15.0f};
    PatchFoodSource source(cfg, 800, 800);

    std::vector<Food> food;
    std::mt19937 rng(42);
    source.pre_seed(food, rng);

    CHECK(food.size() == 50);  // 2 patches * 25 per patch
}

TEST_CASE("PatchFoodSource: food is clustered", "[food_source]") {
    PatchFoodConfig cfg{1, 50, 30.0f, 10.0f};
    PatchFoodSource source(cfg, 2000, 2000);

    std::vector<Food> food;
    std::mt19937 rng(42);
    source.pre_seed(food, rng);

    REQUIRE(food.size() == 50);

    // Compute mean position
    float mean_x = 0, mean_y = 0;
    for (const auto& f : food) {
        mean_x += f.position.x;
        mean_y += f.position.y;
    }
    mean_x /= 50.0f;
    mean_y /= 50.0f;

    // Most food should be within 3 * patch_radius of the mean
    // (99.7% for normal distribution, but wrapping may shift some)
    int close_count = 0;
    float threshold = 3.0f * 30.0f;  // 3 sigma
    for (const auto& f : food) {
        float dx = std::abs(f.position.x - mean_x);
        float dy = std::abs(f.position.y - mean_y);
        if (dx < threshold && dy < threshold) ++close_count;
    }
    CHECK(close_count >= 40);  // at least 80% should be close
}

TEST_CASE("PatchFoodSource: new patch spawns when one is depleted", "[food_source]") {
    PatchFoodConfig cfg{2, 30, 40.0f, 10.0f};
    PatchFoodSource source(cfg, 800, 800);

    std::vector<Food> food;
    std::mt19937 rng(42);
    source.pre_seed(food, rng);
    REQUIRE(food.size() == 60);

    // Remove 30 items (simulate one patch being eaten)
    food.erase(food.begin(), food.begin() + 30);
    CHECK(food.size() == 30);

    // spawn should detect deficit and create a new patch
    source.spawn(food, 0.01f, rng);
    CHECK(food.size() == 60);
}

TEST_CASE("PatchFoodSource: does not over-spawn", "[food_source]") {
    PatchFoodConfig cfg{2, 30, 40.0f, 10.0f};
    PatchFoodSource source(cfg, 800, 800);

    std::vector<Food> food;
    std::mt19937 rng(42);
    source.pre_seed(food, rng);
    REQUIRE(food.size() == 60);

    // No food eaten → spawn should add nothing
    source.spawn(food, 1.0f, rng);
    CHECK(food.size() == 60);
}

TEST_CASE("PatchFoodSource: partial depletion does not trigger new patch", "[food_source]") {
    PatchFoodConfig cfg{2, 30, 40.0f, 10.0f};
    PatchFoodSource source(cfg, 800, 800);

    std::vector<Food> food;
    std::mt19937 rng(42);
    source.pre_seed(food, rng);

    // Remove only 10 items (less than one patch)
    food.erase(food.begin(), food.begin() + 10);
    CHECK(food.size() == 50);

    source.spawn(food, 0.01f, rng);
    // Still 50 — not enough depletion for a new patch
    CHECK(food.size() == 50);
}

// --- Integration with World ---

TEST_CASE("World with patch food source: food stays near target", "[food_source]") {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.food_eat_radius = 8.0f;
    config.food_source_config = PatchFoodConfig{2, 20, 40.0f, 10.0f};

    World world(config);

    std::mt19937 rng(42);
    world.pre_seed_food(rng);

    CHECK(world.get_food().size() == 40);  // 2 * 20

    // Step without boids — food count should remain stable
    for (int i = 0; i < 100; ++i) {
        world.step(1.0f / 60.0f, &rng);
    }
    CHECK(world.get_food().size() == 40);
}

TEST_CASE("World backward compat: flat config fields still work", "[food_source]") {
    WorldConfig config;
    config.width = 400;
    config.height = 400;
    config.food_spawn_rate = 10000.0f;  // very high to guarantee spawning
    config.food_max = 50;
    config.food_energy = 20.0f;
    // food_source_config left as default UniformFoodConfig — should sync from flat fields

    World world(config);

    std::mt19937 rng(42);
    world.step(1.0f, &rng);

    // Should have spawned food up to max
    CHECK(world.get_food().size() == 50);
    CHECK(world.get_food()[0].energy_value == 20.0f);
}
