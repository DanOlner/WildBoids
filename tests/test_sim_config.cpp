#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "io/sim_config.h"
#include <filesystem>
#include <fstream>

using Catch::Matchers::WithinAbs;

static std::string data_path(const std::string& filename) {
    for (const auto& prefix : {"data/", "../data/", "../../data/"}) {
        std::string path = std::string(prefix) + filename;
        if (std::filesystem::exists(path)) return path;
    }
    return "data/" + filename;
}

TEST_CASE("Load sim_config.json", "[sim_config]") {
    SimConfig cfg = load_sim_config(data_path("sim_config.json"));

    // World — check fields loaded (positive values, not defaults)
    CHECK(cfg.world.width > 0);
    CHECK(cfg.world.height > 0);
    CHECK(cfg.world.linear_drag >= 0);
    CHECK(cfg.world.angular_drag >= 0);
    CHECK(cfg.world.grid_cell_size > 0);

    // Food — eat radius loaded
    CHECK(cfg.world.food_eat_radius > 0);

    // Energy
    CHECK(cfg.world.metabolism_rate >= 0);
    CHECK(cfg.world.thrust_cost >= 0);

    // Evolution
    CHECK(cfg.neat.population_size > 0);
    CHECK(cfg.ticks_per_generation > 0);
    CHECK(cfg.generations > 0);
    CHECK(cfg.save_interval > 0);

    // NEAT
    CHECK(cfg.neat.add_node_prob > 0);
    CHECK(cfg.neat.add_connection_prob > 0);
    CHECK(cfg.neat.weight_mutate_prob > 0);
    CHECK(cfg.neat.elitism >= 0);
    CHECK(cfg.neat.max_stagnation > 0);
}

TEST_CASE("Sim config uses WorldConfig defaults for missing fields", "[sim_config]") {
    std::string tmp_path = "test_minimal_sim_config.json";
    {
        std::ofstream f(tmp_path);
        f << R"({"world": {"width": 500}})";
    }

    SimConfig cfg = load_sim_config(tmp_path);
    CHECK_THAT(cfg.world.width, WithinAbs(500.0f, 1e-3f));
    // Height should be the WorldConfig default (1000)
    CHECK_THAT(cfg.world.height, WithinAbs(1000.0f, 1e-3f));
    // Food fields should be WorldConfig defaults
    CHECK(cfg.world.food_max == 100);

    std::filesystem::remove(tmp_path);
}

TEST_CASE("Sim config throws on missing file", "[sim_config]") {
    CHECK_THROWS(load_sim_config("nonexistent_config.json"));
}

TEST_CASE("Sim config: uniform food mode parsed", "[sim_config]") {
    std::string tmp_path = "test_uniform_food_config.json";
    {
        std::ofstream f(tmp_path);
        f << R"({"food": {"mode": "uniform", "spawnRate": 7.0, "max": 50, "energy": 20.0}})";
    }

    SimConfig cfg = load_sim_config(tmp_path);
    auto* uc = std::get_if<UniformFoodConfig>(&cfg.world.food_source_config);
    REQUIRE(uc != nullptr);
    CHECK_THAT(uc->spawn_rate, WithinAbs(7.0f, 1e-3f));
    CHECK(uc->max_food == 50);
    CHECK_THAT(uc->energy, WithinAbs(20.0f, 1e-3f));

    std::filesystem::remove(tmp_path);
}

TEST_CASE("Sim config: patch food mode parsed", "[sim_config]") {
    std::string tmp_path = "test_patch_food_config.json";
    {
        std::ofstream f(tmp_path);
        f << R"({"food": {"mode": "patches", "numPatches": 3, "foodPerPatch": 40, "patchRadius": 60.0, "energy": 12.0}})";
    }

    SimConfig cfg = load_sim_config(tmp_path);
    auto* pc = std::get_if<PatchFoodConfig>(&cfg.world.food_source_config);
    REQUIRE(pc != nullptr);
    CHECK(pc->num_patches == 3);
    CHECK(pc->food_per_patch == 40);
    CHECK_THAT(pc->patch_radius, WithinAbs(60.0f, 1e-3f));
    CHECK_THAT(pc->energy, WithinAbs(12.0f, 1e-3f));

    std::filesystem::remove(tmp_path);
}

TEST_CASE("Sim config: missing mode defaults to uniform", "[sim_config]") {
    std::string tmp_path = "test_default_food_config.json";
    {
        std::ofstream f(tmp_path);
        f << R"({"food": {"spawnRate": 3.0, "max": 60}})";
    }

    SimConfig cfg = load_sim_config(tmp_path);
    auto* uc = std::get_if<UniformFoodConfig>(&cfg.world.food_source_config);
    REQUIRE(uc != nullptr);
    CHECK_THAT(uc->spawn_rate, WithinAbs(3.0f, 1e-3f));
    CHECK(uc->max_food == 60);

    std::filesystem::remove(tmp_path);
}
