#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "brain/neat_genome.h"
#include "brain/neat_network.h"
#include "brain/population.h"
#include "io/boid_spec.h"
#include "simulation/world.h"
#include <cmath>
#include <filesystem>
#include <numeric>
#include <random>

using Catch::Matchers::WithinAbs;

// --- Helpers (same pattern as test_evolution.cpp and headless_main.cpp) ---

static BoidSpec make_prey_spec() {
    BoidSpec spec;
    spec.version = "0.2";
    spec.type = "prey";
    spec.mass = 1.0f;
    spec.moment_of_inertia = 0.5f;
    spec.initial_energy = 100.0f;

    spec.thrusters.push_back({0, "rear",       {0.0f, -0.5f}, {0.0f, 1.0f},  50.0f});
    spec.thrusters.push_back({1, "left_rear",  {-0.3f, -0.4f}, {1.0f, 0.0f}, 20.0f});
    spec.thrusters.push_back({2, "right_rear", {0.3f, -0.4f}, {-1.0f, 0.0f}, 20.0f});
    spec.thrusters.push_back({3, "front",      {0.0f, 0.5f},  {0.0f, -1.0f}, 15.0f});

    constexpr float DEG = static_cast<float>(M_PI) / 180.0f;
    spec.sensors.push_back({0,   0.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({1, -36.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({2,  36.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({3, -72.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({4,  72.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({5, -135.0f * DEG, 90.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({6,  135.0f * DEG, 90.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({7,    0.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Food, SignalType::NearestDistance});
    spec.sensors.push_back({8, -120.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Food, SignalType::NearestDistance});
    spec.sensors.push_back({9,  120.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Food, SignalType::NearestDistance});

    return spec;
}

static WorldConfig make_evolution_config() {
    WorldConfig config;
    config.width = 800.0f;
    config.height = 800.0f;
    config.toroidal = true;
    config.linear_drag = 0.02f;
    config.angular_drag = 0.05f;
    config.grid_cell_size = 100.0f;
    config.food_spawn_rate = 5.0f;
    config.food_max = 80;
    config.food_eat_radius = 10.0f;
    config.food_energy = 15.0f;
    config.metabolism_rate = 0.1f;
    config.thrust_cost = 0.01f;
    return config;
}

static std::vector<float> run_generation(
    const std::vector<NeatGenome>& genomes,
    const BoidSpec& base_spec,
    const WorldConfig& config,
    int ticks,
    std::mt19937& rng)
{
    World world(config);

    std::uniform_real_distribution<float> x_dist(0.0f, config.width);
    std::uniform_real_distribution<float> y_dist(0.0f, config.height);
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * static_cast<float>(M_PI));

    for (int i = 0; i < config.food_max / 2; ++i) {
        world.add_food(Food{Vec2{x_dist(rng), y_dist(rng)}, config.food_energy});
    }

    for (const auto& genome : genomes) {
        Boid boid = create_boid_from_spec(base_spec);
        boid.body.position = Vec2{x_dist(rng), y_dist(rng)};
        boid.body.angle = angle_dist(rng);
        boid.brain = std::make_unique<NeatNetwork>(genome);
        world.add_boid(std::move(boid));
    }

    float dt = 1.0f / 120.0f;
    for (int t = 0; t < ticks; ++t) {
        world.step(dt, &rng);
    }

    std::vector<float> fitness(genomes.size());
    const auto& boids = world.get_boids();
    for (int i = 0; i < static_cast<int>(genomes.size()); ++i) {
        fitness[i] = boids[i].total_energy_gained;
    }
    return fitness;
}

// --- Tests ---

TEST_CASE("Headless: completes N generations without crash", "[headless]") {
    BoidSpec spec = make_prey_spec();
    WorldConfig config = make_evolution_config();

    int next_innov = 1;
    NeatGenome seed = NeatGenome::minimal(10, 4, next_innov);

    PopulationParams params;
    params.population_size = 20;
    params.add_node_prob = 0.03f;
    params.add_connection_prob = 0.05f;
    params.weight_mutate_prob = 0.8f;
    params.weight_sigma = 0.5f;
    params.crossover_prob = 0.75f;
    params.compat_threshold = 3.0f;
    params.survival_rate = 0.25f;
    params.elitism = 1;
    params.max_stagnation = 15;

    std::mt19937 rng(42);
    Population pop(seed, params, rng);

    constexpr int TICKS_PER_GEN = 600;
    constexpr int NUM_GENS = 15;

    for (int gen = 0; gen < NUM_GENS; ++gen) {
        auto fitness = run_generation(pop.genomes(), spec, config, TICKS_PER_GEN, rng);

        pop.evaluate([&](int idx, const NeatGenome&) {
            return fitness[idx];
        });

        // Population size should be stable
        CHECK(pop.size() == params.population_size);
        // Species count should be positive
        CHECK(pop.species_count() > 0);
        // Best fitness should be finite
        CHECK(std::isfinite(pop.best_fitness()));

        if (gen < NUM_GENS - 1) {
            pop.advance_generation();
        }
    }
}

TEST_CASE("Headless: saved champion genome loads and produces valid network", "[headless]") {
    BoidSpec spec = make_prey_spec();
    WorldConfig config = make_evolution_config();

    int next_innov = 1;
    NeatGenome seed = NeatGenome::minimal(10, 4, next_innov);

    PopulationParams params;
    params.population_size = 20;
    params.add_node_prob = 0.05f;
    params.add_connection_prob = 0.08f;
    params.weight_mutate_prob = 0.8f;
    params.weight_sigma = 0.5f;
    params.compat_threshold = 3.0f;

    std::mt19937 rng(123);
    Population pop(seed, params, rng);

    constexpr int TICKS_PER_GEN = 600;

    // Run a few generations to get mutations
    for (int gen = 0; gen < 5; ++gen) {
        auto fitness = run_generation(pop.genomes(), spec, config, TICKS_PER_GEN, rng);
        pop.evaluate([&](int idx, const NeatGenome&) { return fitness[idx]; });
        pop.advance_generation();
    }

    // Save champion
    BoidSpec champion_spec = spec;
    champion_spec.genome = pop.best_genome();
    std::string tmp_path = "test_headless_champion.json";
    save_boid_spec(champion_spec, tmp_path);

    // Reload and verify
    BoidSpec reloaded = load_boid_spec(tmp_path);
    REQUIRE(reloaded.genome.has_value());

    // Build network from reloaded genome
    NeatNetwork net(*reloaded.genome);
    float inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float outputs[4] = {};
    net.activate(inputs, 10, outputs, 4);

    for (int j = 0; j < 4; ++j) {
        CHECK(std::isfinite(outputs[j]));
        CHECK(outputs[j] >= 0.0f);
        CHECK(outputs[j] <= 1.0f);
    }

    // Champion should also create a working boid
    Boid boid = create_boid_from_spec(reloaded);
    CHECK(boid.brain != nullptr);

    std::filesystem::remove(tmp_path);
}

TEST_CASE("Headless: champion genome replays in world without crash", "[headless]") {
    BoidSpec spec = make_prey_spec();
    WorldConfig config = make_evolution_config();

    int next_innov = 1;
    NeatGenome seed = NeatGenome::minimal(10, 4, next_innov);

    PopulationParams params;
    params.population_size = 20;
    params.add_node_prob = 0.05f;
    params.add_connection_prob = 0.08f;
    params.weight_mutate_prob = 0.8f;
    params.weight_sigma = 0.5f;

    std::mt19937 rng(99);
    Population pop(seed, params, rng);

    // Evolve for a few generations
    for (int gen = 0; gen < 5; ++gen) {
        auto fitness = run_generation(pop.genomes(), spec, config, 600, rng);
        pop.evaluate([&](int idx, const NeatGenome&) { return fitness[idx]; });
        pop.advance_generation();
    }

    // Take champion and replay it solo in a fresh world
    NeatGenome champion = pop.best_genome();
    World world(config);

    std::uniform_real_distribution<float> x_dist(0.0f, config.width);
    std::uniform_real_distribution<float> y_dist(0.0f, config.height);

    // Seed some food
    for (int i = 0; i < 40; ++i) {
        world.add_food(Food{Vec2{x_dist(rng), y_dist(rng)}, config.food_energy});
    }

    Boid boid = create_boid_from_spec(spec);
    boid.body.position = Vec2{400, 400};
    boid.body.angle = 0;
    boid.brain = std::make_unique<NeatNetwork>(champion);
    world.add_boid(std::move(boid));

    // Run for 1000 ticks — should not crash
    for (int t = 0; t < 1000; ++t) {
        world.step(1.0f / 120.0f, &rng);
    }

    const auto& b = world.get_boids()[0];
    CHECK(std::isfinite(b.body.position.x));
    CHECK(std::isfinite(b.body.position.y));
}
