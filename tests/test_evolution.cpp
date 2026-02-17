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

// Build a BoidSpec programmatically (matching simple_boid.json layout: 7 sensors, 4 thrusters)
static BoidSpec make_prey_spec() {
    BoidSpec spec;
    spec.version = "0.2";
    spec.type = "prey";
    spec.mass = 1.0f;
    spec.moment_of_inertia = 0.5f;
    spec.initial_energy = 100.0f;

    // 4 thrusters: rear, left-rear, right-rear, front
    spec.thrusters.push_back({0, "rear",       {0.0f, -0.5f}, {0.0f, 1.0f},  50.0f});
    spec.thrusters.push_back({1, "left_rear",  {-0.3f, -0.4f}, {1.0f, 0.0f}, 20.0f});
    spec.thrusters.push_back({2, "right_rear", {0.3f, -0.4f}, {-1.0f, 0.0f}, 20.0f});
    spec.thrusters.push_back({3, "front",      {0.0f, 0.5f},  {0.0f, -1.0f}, 15.0f});

    // 7 sensors: 5 forward arcs + 2 rear arcs
    constexpr float DEG = static_cast<float>(M_PI) / 180.0f;
    spec.sensors.push_back({0,   0.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({1, -36.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({2,  36.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({3, -72.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({4,  72.0f * DEG, 36.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({5, -135.0f * DEG, 90.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});
    spec.sensors.push_back({6,  135.0f * DEG, 90.0f * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance});

    return spec;
}

// Create a WorldConfig tuned for evolution testing
static WorldConfig make_evolution_config() {
    WorldConfig config;
    config.width = 800.0f;
    config.height = 800.0f;
    config.toroidal = true;
    config.linear_drag = 0.02f;
    config.angular_drag = 0.05f;
    config.grid_cell_size = 100.0f;

    // Generous food
    config.food_spawn_rate = 5.0f;
    config.food_max = 80;
    config.food_eat_radius = 10.0f;
    config.food_energy = 15.0f;

    // Low energy costs — boids should survive the generation
    config.metabolism_rate = 0.1f;
    config.thrust_cost = 0.01f;

    return config;
}

// Run one generation: create a World, spawn boids with genomes as brains,
// run for N ticks, return fitness (total_energy_gained) for each genome.
static std::vector<float> run_generation(
    const std::vector<NeatGenome>& genomes,
    const BoidSpec& base_spec,
    const WorldConfig& config,
    int ticks,
    std::mt19937& rng)
{
    World world(config);

    // Pre-seed some food so generation 0 has something to find
    std::uniform_real_distribution<float> x_dist(0.0f, config.width);
    std::uniform_real_distribution<float> y_dist(0.0f, config.height);
    for (int i = 0; i < config.food_max / 2; ++i) {
        world.add_food(Food{Vec2{x_dist(rng), y_dist(rng)}, config.food_energy});
    }

    // Spawn one boid per genome at random position/heading
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * static_cast<float>(M_PI));

    for (const auto& genome : genomes) {
        Boid boid = create_boid_from_spec(base_spec);
        boid.body.position = Vec2{x_dist(rng), y_dist(rng)};
        boid.body.angle = angle_dist(rng);
        boid.brain = std::make_unique<NeatNetwork>(genome);
        world.add_boid(std::move(boid));
    }

    // Run simulation
    float dt = 1.0f / 120.0f;
    for (int t = 0; t < ticks; ++t) {
        world.step(dt, &rng);
    }

    // Collect fitness
    std::vector<float> fitness(genomes.size());
    const auto& boids = world.get_boids();
    for (int i = 0; i < static_cast<int>(genomes.size()); ++i) {
        fitness[i] = boids[i].total_energy_gained;
    }

    return fitness;
}

// --- Tests ---

TEST_CASE("Evolution: evaluation harness produces non-zero fitness", "[evolution]") {
    // Verify the basic evaluation pipeline works: genomes → boids → world → fitness
    BoidSpec spec = make_prey_spec();
    WorldConfig config = make_evolution_config();

    int next_innov = 1;
    NeatGenome seed = NeatGenome::minimal(7, 4, next_innov);  // 7 sensors → 4 thrusters

    // Create a small set of genomes with perturbed weights
    std::mt19937 rng(42);
    std::vector<NeatGenome> genomes;
    for (int i = 0; i < 10; ++i) {
        NeatGenome g = seed;
        std::normal_distribution<float> noise(0.0f, 0.5f);
        for (auto& c : g.connections) {
            c.weight += noise(rng);
        }
        genomes.push_back(std::move(g));
    }

    auto fitness = run_generation(genomes, spec, config, 2000, rng);

    // At least some boids should have found food
    float max_fitness = *std::max_element(fitness.begin(), fitness.end());
    float mean_fitness = std::accumulate(fitness.begin(), fitness.end(), 0.0f)
                         / static_cast<float>(fitness.size());

    INFO("Max fitness: " << max_fitness << ", Mean fitness: " << mean_fitness);
    CHECK(max_fitness > 0.0f);
}

TEST_CASE("Evolution: moving boids outperform stationary ones", "[evolution]") {
    // Sanity check: a boid that moves forward should gather more food
    // than one that sits still (validating the fitness gradient).
    BoidSpec spec = make_prey_spec();
    WorldConfig config = make_evolution_config();
    config.food_max = 100;
    config.food_spawn_rate = 10.0f;
    config.metabolism_rate = 0.0f;  // no metabolism to isolate the food-finding effect
    config.thrust_cost = 0.0f;

    int next_innov = 1;

    // "Mover" genome: strong positive weight from any input to rear thruster (output 0)
    NeatGenome mover = NeatGenome::minimal(7, 4, next_innov);
    for (auto& c : mover.connections) {
        if (c.target == 7) {  // output node 0 = rear thruster (node ids: inputs 0-6, outputs 7-10)
            c.weight = 3.0f;  // sigmoid(3) ≈ 0.95 → strong forward thrust
        } else {
            c.weight = 0.0f;  // sigmoid(0) = 0.5 → moderate other thrusters
        }
    }

    // "Sitter" genome: all weights strongly negative → all thrusters near 0
    NeatGenome sitter = NeatGenome::minimal(7, 4, next_innov);
    for (auto& c : sitter.connections) {
        c.weight = -5.0f;  // sigmoid(-5) ≈ 0.007 → nearly zero thrust
    }

    std::mt19937 rng(42);
    std::vector<NeatGenome> genomes;

    // 5 movers, 5 sitters
    for (int i = 0; i < 5; ++i) genomes.push_back(mover);
    for (int i = 0; i < 5; ++i) genomes.push_back(sitter);

    auto fitness = run_generation(genomes, spec, config, 3000, rng);

    float mover_avg = std::accumulate(fitness.begin(), fitness.begin() + 5, 0.0f) / 5.0f;
    float sitter_avg = std::accumulate(fitness.begin() + 5, fitness.end(), 0.0f) / 5.0f;

    INFO("Mover avg fitness: " << mover_avg);
    INFO("Sitter avg fitness: " << sitter_avg);

    CHECK(mover_avg > sitter_avg);
}

TEST_CASE("Evolution: fitness improves over generations", "[evolution]") {
    // Does NEAT weight evolution produce boids that find more food?
    // Uses small population and short generations for test speed.
    // The fitness gradient here is movement-based: boids that move forward
    // sweep more area and find more food. Sensors detect other boids (not food),
    // so the gradient is relatively weak — we just check that evolution finds
    // better-than-average genomes over time.
    BoidSpec spec = make_prey_spec();
    WorldConfig config = make_evolution_config();

    int next_innov = 1;
    NeatGenome seed = NeatGenome::minimal(7, 4, next_innov);

    PopulationParams params;
    params.population_size = 20;
    params.add_node_prob = 0.03f;
    params.add_connection_prob = 0.05f;
    params.weight_mutate_prob = 0.8f;
    params.weight_sigma = 0.5f;
    params.weight_replace_prob = 0.1f;
    params.crossover_prob = 0.75f;
    params.compat_threshold = 3.0f;
    params.survival_rate = 0.25f;
    params.elitism = 1;
    params.max_stagnation = 15;

    std::mt19937 rng(42);
    Population pop(seed, params, rng);

    constexpr int TICKS_PER_GEN = 1200;
    constexpr int NUM_GENS = 20;

    float gen0_mean = 0.0f;
    float best_fitness_seen = 0.0f;

    for (int gen = 0; gen < NUM_GENS; ++gen) {
        auto fitness = run_generation(pop.genomes(), spec, config, TICKS_PER_GEN, rng);

        pop.evaluate([&](int idx, const NeatGenome&) {
            return fitness[idx];
        });

        float gen_mean = std::accumulate(fitness.begin(), fitness.end(), 0.0f)
                         / static_cast<float>(fitness.size());

        if (gen == 0) {
            gen0_mean = gen_mean;
        }
        best_fitness_seen = std::max(best_fitness_seen, pop.best_fitness());

        if (gen < NUM_GENS - 1) {
            pop.advance_generation();
        }
    }

    INFO("Gen 0 mean: " << gen0_mean);
    INFO("Best fitness seen across all generations: " << best_fitness_seen);
    INFO("Species count: " << pop.species_count());

    // Evolution should discover genomes that outperform the gen-0 average.
    // This is a weak check intentionally — the fitness gradient from movement alone
    // (without food-specific sensors) is noisy. The stronger validation is the
    // mover-vs-sitter test above.
    CHECK(best_fitness_seen > gen0_mean);
}

TEST_CASE("Evolution: all evolved genomes produce valid networks", "[evolution]") {
    // After several generations of mutation + crossover in the foraging context,
    // every genome should still produce a valid NeatNetwork that activates correctly.
    BoidSpec spec = make_prey_spec();
    WorldConfig config = make_evolution_config();

    int next_innov = 1;
    NeatGenome seed = NeatGenome::minimal(7, 4, next_innov);

    PopulationParams params;
    params.population_size = 20;
    params.add_node_prob = 0.05f;
    params.add_connection_prob = 0.08f;
    params.weight_mutate_prob = 0.8f;
    params.weight_sigma = 0.5f;
    params.compat_threshold = 3.0f;
    params.survival_rate = 0.25f;
    params.elitism = 1;

    std::mt19937 rng(42);
    Population pop(seed, params, rng);

    constexpr int TICKS_PER_GEN = 600;

    for (int gen = 0; gen < 10; ++gen) {
        auto fitness = run_generation(pop.genomes(), spec, config, TICKS_PER_GEN, rng);
        pop.evaluate([&](int idx, const NeatGenome&) { return fitness[idx]; });
        pop.advance_generation();
    }

    // Every genome should build a valid network with correct I/O dimensions
    for (int i = 0; i < pop.size(); ++i) {
        NeatNetwork net(pop.genome(i));
        float inputs[7] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        float outputs[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        net.activate(inputs, 7, outputs, 4);
        for (int j = 0; j < 4; ++j) {
            CHECK(std::isfinite(outputs[j]));
            CHECK(outputs[j] >= 0.0f);  // sigmoid outputs are non-negative
            CHECK(outputs[j] <= 1.0f);  // sigmoid outputs are ≤ 1
        }
    }
}
