#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "brain/population.h"
#include "brain/neat_genome.h"
#include "brain/neat_network.h"
#include "simulation/world.h"
#include "simulation/sensor.h"
#include "io/boid_spec.h"
#include <random>
#include <numeric>
#include <cmath>

using Catch::Matchers::WithinAbs;

// --- Helpers ---

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
    // 7 boid sensors
    spec.sensors.push_back({0,   0.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({1, -36.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({2,  36.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({3, -72.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({4,  72.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({5, -135.0f * DEG, 90.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({6,  135.0f * DEG, 90.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    // 3 food sensors
    spec.sensors.push_back({7,    0.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Food, SignalType::NearestDistance});
    spec.sensors.push_back({8, -120.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Food, SignalType::NearestDistance});
    spec.sensors.push_back({9,  120.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Food, SignalType::NearestDistance});

    return spec;
}

static BoidSpec make_predator_spec() {
    BoidSpec spec;
    spec.version = "0.2";
    spec.type = "predator";
    spec.mass = 1.0f;
    spec.moment_of_inertia = 0.5f;
    spec.initial_energy = 100.0f;

    spec.thrusters.push_back({0, "rear",       {0.0f, -0.5f}, {0.0f, 1.0f},  50.0f});
    spec.thrusters.push_back({1, "left_rear",  {-0.3f, -0.4f}, {1.0f, 0.0f}, 20.0f});
    spec.thrusters.push_back({2, "right_rear", {0.3f, -0.4f}, {-1.0f, 0.0f}, 20.0f});
    spec.thrusters.push_back({3, "front",      {0.0f, 0.5f},  {0.0f, -1.0f}, 15.0f});

    constexpr float DEG = static_cast<float>(M_PI) / 180.0f;
    // 7 boid sensors (same as prey)
    spec.sensors.push_back({0,   0.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({1, -36.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({2,  36.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({3, -72.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({4,  72.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({5, -135.0f * DEG, 90.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({6,  135.0f * DEG, 90.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    // 3 prey sensors (instead of food sensors)
    spec.sensors.push_back({7,    0.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Prey, SignalType::NearestDistance});
    spec.sensors.push_back({8, -120.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Prey, SignalType::NearestDistance});
    spec.sensors.push_back({9,  120.0f * DEG, 120.0f * DEG, 120.0f, EntityFilter::Prey, SignalType::NearestDistance});

    return spec;
}

static WorldConfig make_coevolution_config() {
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

    config.predator_catch_radius = 15.0f;
    config.predator_catch_energy = 50.0f;

    return config;
}

// Local run_generation for dual populations
struct DualResult {
    std::vector<float> prey_fitness;
    std::vector<float> predator_fitness;
    int prey_survivors = 0;
    int predator_survivors = 0;
};

static DualResult run_dual_generation(
    const std::vector<NeatGenome>& prey_genomes,
    const std::vector<NeatGenome>& predator_genomes,
    const BoidSpec& prey_spec,
    const BoidSpec& predator_spec,
    const WorldConfig& config,
    int ticks,
    std::mt19937& rng)
{
    World world(config);

    std::uniform_real_distribution<float> x_dist(0.0f, config.width);
    std::uniform_real_distribution<float> y_dist(0.0f, config.height);
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * static_cast<float>(M_PI));

    // Pre-seed food
    for (int i = 0; i < config.food_max / 2; ++i) {
        world.add_food(Food{Vec2{x_dist(rng), y_dist(rng)}, config.food_energy});
    }

    // Spawn prey at indices [0, prey_count)
    for (const auto& genome : prey_genomes) {
        Boid boid = create_boid_from_spec(prey_spec);
        boid.body.position = Vec2{x_dist(rng), y_dist(rng)};
        boid.body.angle = angle_dist(rng);
        boid.brain = std::make_unique<NeatNetwork>(genome);
        world.add_boid(std::move(boid));
    }

    // Spawn predators at indices [prey_count, total)
    for (const auto& genome : predator_genomes) {
        Boid boid = create_boid_from_spec(predator_spec);
        boid.body.position = Vec2{x_dist(rng), y_dist(rng)};
        boid.body.angle = angle_dist(rng);
        boid.brain = std::make_unique<NeatNetwork>(genome);
        world.add_boid(std::move(boid));
    }

    int prey_count = static_cast<int>(prey_genomes.size());

    float dt = 1.0f / 120.0f;
    for (int t = 0; t < ticks; ++t) {
        world.step(dt, &rng);
    }

    DualResult result;
    result.prey_fitness.resize(prey_genomes.size());
    result.predator_fitness.resize(predator_genomes.size());

    const auto& boids = world.get_boids();
    for (int i = 0; i < static_cast<int>(prey_genomes.size()); ++i) {
        result.prey_fitness[i] = boids[i].total_energy_gained;
        if (boids[i].alive) ++result.prey_survivors;
    }
    for (int i = 0; i < static_cast<int>(predator_genomes.size()); ++i) {
        result.predator_fitness[i] = boids[prey_count + i].total_energy_gained;
        if (boids[prey_count + i].alive) ++result.predator_survivors;
    }

    return result;
}

// --- Tests ---

TEST_CASE("Dual evolution: both populations produce fitness vectors of correct size", "[coevolution]") {
    BoidSpec prey_spec = make_prey_spec();
    BoidSpec pred_spec = make_predator_spec();
    WorldConfig config = make_coevolution_config();

    int next_innov = 1;
    NeatGenome prey_seed = NeatGenome::minimal(10, 4, next_innov);
    next_innov = 1;
    NeatGenome pred_seed = NeatGenome::minimal(10, 4, next_innov);

    std::mt19937 rng(42);

    // Create small populations
    std::vector<NeatGenome> prey_genomes(5, prey_seed);
    std::vector<NeatGenome> pred_genomes(3, pred_seed);

    // Perturb weights
    std::normal_distribution<float> noise(0.0f, 0.5f);
    for (auto& g : prey_genomes)
        for (auto& c : g.connections) c.weight += noise(rng);
    for (auto& g : pred_genomes)
        for (auto& c : g.connections) c.weight += noise(rng);

    auto result = run_dual_generation(prey_genomes, pred_genomes, prey_spec, pred_spec,
                                       config, 1000, rng);

    CHECK(result.prey_fitness.size() == 5);
    CHECK(result.predator_fitness.size() == 3);
}

TEST_CASE("Dual evolution: predators can catch prey and gain fitness", "[coevolution]") {
    BoidSpec prey_spec = make_prey_spec();
    BoidSpec pred_spec = make_predator_spec();
    WorldConfig config = make_coevolution_config();
    config.predator_catch_radius = 20.0f;  // generous catch radius

    int next_innov = 1;

    // Create "mover" genomes that thrust forward
    NeatGenome mover = NeatGenome::minimal(10, 4, next_innov);
    for (auto& c : mover.connections) {
        if (c.target == 10) {  // rear thruster
            c.weight = 3.0f;
        } else {
            c.weight = 0.0f;
        }
    }

    std::mt19937 rng(42);

    // 10 prey, 5 predators — all movers
    std::vector<NeatGenome> prey_genomes(10, mover);
    std::vector<NeatGenome> pred_genomes(5, mover);

    auto result = run_dual_generation(prey_genomes, pred_genomes, prey_spec, pred_spec,
                                       config, 2000, rng);

    // At least some predators should have caught prey
    float max_pred_fitness = *std::max_element(result.predator_fitness.begin(),
                                                result.predator_fitness.end());
    INFO("Max predator fitness: " << max_pred_fitness);
    CHECK(max_pred_fitness > 0.0f);

    // Some prey should have been killed
    CHECK(result.prey_survivors < 10);
}

TEST_CASE("Dual evolution: two Population instances can advance independently", "[coevolution]") {
    int next_innov = 1;
    NeatGenome prey_seed = NeatGenome::minimal(10, 4, next_innov);
    next_innov = 1;
    NeatGenome pred_seed = NeatGenome::minimal(10, 4, next_innov);

    PopulationParams params;
    params.population_size = 10;
    params.weight_mutate_prob = 0.8f;
    params.crossover_prob = 0.75f;
    params.compat_threshold = 3.0f;
    params.survival_rate = 0.25f;
    params.elitism = 1;

    std::mt19937 rng(42);
    Population prey_pop(prey_seed, params, rng);
    Population pred_pop(pred_seed, params, rng);

    // Evaluate with dummy fitness
    prey_pop.evaluate([](int idx, const NeatGenome&) {
        return static_cast<float>(idx + 1);
    });
    pred_pop.evaluate([](int idx, const NeatGenome&) {
        return static_cast<float>(10 - idx);
    });

    // Both should advance without issues
    prey_pop.advance_generation();
    pred_pop.advance_generation();

    CHECK(prey_pop.size() == 10);
    CHECK(pred_pop.size() == 10);
    CHECK(prey_pop.species_count() >= 1);
    CHECK(pred_pop.species_count() >= 1);
}

TEST_CASE("Dual evolution: predator champion genome produces valid network", "[coevolution]") {
    BoidSpec pred_spec = make_predator_spec();

    int next_innov = 1;
    NeatGenome pred_seed = NeatGenome::minimal(10, 4, next_innov);

    PopulationParams params;
    params.population_size = 10;
    params.add_node_prob = 0.05f;
    params.add_connection_prob = 0.08f;
    params.weight_mutate_prob = 0.8f;
    params.weight_sigma = 0.5f;
    params.compat_threshold = 3.0f;
    params.survival_rate = 0.25f;
    params.elitism = 1;

    std::mt19937 rng(42);
    Population pop(pred_seed, params, rng);

    // Evolve for a few generations with dummy fitness
    for (int gen = 0; gen < 5; ++gen) {
        pop.evaluate([&](int idx, const NeatGenome&) {
            return static_cast<float>(idx + 1);
        });
        pop.advance_generation();
    }

    // Best genome should produce a valid network
    NeatNetwork net(pop.best_genome());
    float inputs[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float outputs[4] = {};
    net.activate(inputs, 10, outputs, 4);

    for (int j = 0; j < 4; ++j) {
        CHECK(std::isfinite(outputs[j]));
        CHECK(outputs[j] >= 0.0f);
        CHECK(outputs[j] <= 1.0f);
    }
}
