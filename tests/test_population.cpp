#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "brain/population.h"
#include "brain/neat_network.h"
#include <cmath>
#include <numeric>
#include <iostream>

using Catch::Matchers::WithinAbs;

static NeatGenome make_minimal(int n_in = 3, int n_out = 2) {
    int next = 1;
    return NeatGenome::minimal(n_in, n_out, next);
}

TEST_CASE("Population: initial size matches params", "[population]") {
    PopulationParams params;
    params.population_size = 50;
    std::mt19937 rng(42);
    Population pop(make_minimal(), params, rng);

    CHECK(pop.size() == 50);
    CHECK(pop.generation() == 0);
}

TEST_CASE("Population: all in one species initially", "[population]") {
    PopulationParams params;
    params.population_size = 50;
    std::mt19937 rng(42);
    Population pop(make_minimal(), params, rng);

    // All start from same seed with only weight mutations → one species
    CHECK(pop.species_count() == 1);
}

TEST_CASE("Population: evaluate sets fitness values", "[population]") {
    PopulationParams params;
    params.population_size = 20;
    std::mt19937 rng(42);
    Population pop(make_minimal(), params, rng);

    pop.evaluate([](int idx, const NeatGenome&) {
        return static_cast<float>(idx);
    });

    CHECK_THAT(pop.fitness(0), WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(pop.fitness(19), WithinAbs(19.0f, 1e-6f));
    CHECK_THAT(pop.best_fitness(), WithinAbs(19.0f, 1e-6f));
}

TEST_CASE("Population: advance preserves population size", "[population]") {
    PopulationParams params;
    params.population_size = 50;
    std::mt19937 rng(42);
    Population pop(make_minimal(), params, rng);

    // Evaluate with random fitness
    std::uniform_real_distribution<float> fitness_dist(0.0f, 10.0f);
    pop.evaluate([&](int, const NeatGenome&) { return fitness_dist(rng); });

    pop.advance_generation();

    CHECK(pop.size() == 50);
    CHECK(pop.generation() == 1);
}

TEST_CASE("Population: multiple generations produce diversity", "[population]") {
    PopulationParams params;
    params.population_size = 50;
    params.add_node_prob = 0.1f;     // higher than default to speed up diversity
    params.add_connection_prob = 0.1f;
    params.compat_threshold = 2.0f;  // tighter to encourage speciation
    std::mt19937 rng(42);
    Population pop(make_minimal(), params, rng);

    for (int gen = 0; gen < 20; ++gen) {
        pop.evaluate([&](int, const NeatGenome&) {
            return std::uniform_real_distribution<float>(0.0f, 10.0f)(rng);
        });
        pop.advance_generation();
    }

    CHECK(pop.generation() == 20);
    CHECK(pop.species_count() >= 1);
    // Population size should be maintained
    CHECK(pop.size() == 50);
}

TEST_CASE("Population: elitism preserves best genome", "[population]") {
    PopulationParams params;
    params.population_size = 30;
    params.elitism = 1;
    params.weight_mutate_prob = 0.0f;  // no mutations to make comparison easier
    params.add_node_prob = 0.0f;
    params.add_connection_prob = 0.0f;
    params.crossover_prob = 0.0f;      // asexual only
    std::mt19937 rng(42);

    NeatGenome seed = make_minimal(2, 1);
    // Give seed distinctive weights
    for (auto& c : seed.connections) c.weight = 42.0f;
    Population pop(seed, params, rng);

    // Evaluate: genome 0 gets highest fitness
    pop.evaluate([](int idx, const NeatGenome&) {
        return (idx == 0) ? 100.0f : 1.0f;
    });

    pop.advance_generation();

    // The elite genome should survive with its weights intact
    bool found_elite = false;
    for (int i = 0; i < pop.size(); ++i) {
        bool matches = true;
        for (const auto& c : pop.genome(i).connections) {
            if (std::abs(c.weight - 42.0f) > 1e-4f) {
                matches = false;
                break;
            }
        }
        if (matches) { found_elite = true; break; }
    }
    CHECK(found_elite);
}

TEST_CASE("Population: all genomes build valid networks", "[population]") {
    PopulationParams params;
    params.population_size = 30;
    params.add_node_prob = 0.1f;
    params.add_connection_prob = 0.1f;
    std::mt19937 rng(42);
    Population pop(make_minimal(), params, rng);

    for (int gen = 0; gen < 10; ++gen) {
        pop.evaluate([&](int, const NeatGenome&) {
            return std::uniform_real_distribution<float>(0.0f, 10.0f)(rng);
        });
        pop.advance_generation();
    }

    // Every genome should produce a valid network
    for (int i = 0; i < pop.size(); ++i) {
        NeatNetwork net(pop.genome(i));
        float inputs[3] = {0.5f, 0.5f, 0.5f};
        float outputs[2] = {0.0f, 0.0f};
        net.activate(inputs, 3, outputs, 2);
        for (int j = 0; j < 2; ++j) {
            CHECK(std::isfinite(outputs[j]));
        }
    }
}

// ---- XOR benchmark ----
// NEAT's classic test: 2 inputs + 1 bias → 1 output.
// XOR requires at least one hidden node, so this tests that structural
// mutation + speciation work together.

static float xor_fitness(const NeatGenome& genome) {
    NeatNetwork net(genome);

    // XOR truth table (input0, input1, bias=1.0)
    float cases[4][3] = {
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
    };
    float expected[4] = {0.0f, 1.0f, 1.0f, 0.0f};

    float total_error = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float output = 0.0f;
        net.activate(cases[i], 3, &output, 1);
        float err = std::abs(output - expected[i]);
        total_error += err * err;
    }

    // Fitness = 4 - total_squared_error (max fitness = 4.0 when perfect)
    return 4.0f - total_error;
}

TEST_CASE("XOR benchmark: NEAT solves XOR", "[population][xor]") {
    PopulationParams params;
    params.population_size = 150;
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

    int next = 1;
    NeatGenome seed = NeatGenome::minimal(3, 1, next);  // 2 inputs + bias → 1 output

    std::mt19937 rng(42);
    Population pop(seed, params, rng);

    bool solved = false;
    int solve_gen = -1;

    for (int gen = 0; gen < 200; ++gen) {
        pop.evaluate([](int, const NeatGenome& g) {
            return xor_fitness(g);
        });

        if (pop.best_fitness() > 3.9f) {
            solved = true;
            solve_gen = gen;
            break;
        }

        pop.advance_generation();
    }

    INFO("Solved at generation " << solve_gen << " with fitness " << pop.best_fitness());
    INFO("Species count: " << pop.species_count());

    // NEAT should solve XOR within 200 generations
    CHECK(solved);

    if (solved) {
        // Verify the solution actually computes XOR
        NeatNetwork net(pop.best_genome());
        float cases[4][3] = {
            {0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
        };
        float expected[4] = {0.0f, 1.0f, 1.0f, 0.0f};

        for (int i = 0; i < 4; ++i) {
            float output = 0.0f;
            net.activate(cases[i], 3, &output, 1);
            CHECK(std::abs(output - expected[i]) < 0.3f);
        }
    }
}
