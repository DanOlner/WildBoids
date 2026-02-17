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

    // World
    CHECK_THAT(cfg.world.width, WithinAbs(800.0f, 1e-3f));
    CHECK_THAT(cfg.world.height, WithinAbs(800.0f, 1e-3f));
    CHECK(cfg.world.toroidal == true);
    CHECK_THAT(cfg.world.linear_drag, WithinAbs(0.02f, 1e-6f));
    CHECK_THAT(cfg.world.angular_drag, WithinAbs(0.05f, 1e-6f));
    CHECK_THAT(cfg.world.grid_cell_size, WithinAbs(100.0f, 1e-3f));

    // Food
    CHECK_THAT(cfg.world.food_spawn_rate, WithinAbs(5.0f, 1e-3f));
    CHECK(cfg.world.food_max == 80);
    CHECK_THAT(cfg.world.food_eat_radius, WithinAbs(10.0f, 1e-3f));
    CHECK_THAT(cfg.world.food_energy, WithinAbs(15.0f, 1e-3f));

    // Energy
    CHECK_THAT(cfg.world.metabolism_rate, WithinAbs(0.1f, 1e-6f));
    CHECK_THAT(cfg.world.thrust_cost, WithinAbs(0.01f, 1e-6f));

    // Evolution
    CHECK(cfg.neat.population_size == 150);
    CHECK(cfg.ticks_per_generation == 2400);
    CHECK(cfg.generations == 100);
    CHECK(cfg.save_interval == 10);

    // NEAT
    CHECK_THAT(cfg.neat.add_node_prob, WithinAbs(0.03f, 1e-6f));
    CHECK_THAT(cfg.neat.add_connection_prob, WithinAbs(0.05f, 1e-6f));
    CHECK_THAT(cfg.neat.weight_mutate_prob, WithinAbs(0.8f, 1e-6f));
    CHECK_THAT(cfg.neat.compat_threshold, WithinAbs(3.0f, 1e-3f));
    CHECK(cfg.neat.elitism == 1);
    CHECK(cfg.neat.max_stagnation == 15);
}

TEST_CASE("Sim config uses WorldConfig defaults for missing fields", "[sim_config]") {
    // Write a minimal config with just one field
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
