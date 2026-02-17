#include "io/sim_config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

SimConfig load_sim_config(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open sim config file: " + path);
    }

    json j = json::parse(file);
    SimConfig cfg;

    // World
    if (j.contains("world")) {
        const auto& w = j["world"];
        cfg.world.width = w.value("width", cfg.world.width);
        cfg.world.height = w.value("height", cfg.world.height);
        cfg.world.toroidal = w.value("toroidal", cfg.world.toroidal);
        cfg.world.linear_drag = w.value("linearDrag", cfg.world.linear_drag);
        cfg.world.angular_drag = w.value("angularDrag", cfg.world.angular_drag);
        cfg.world.grid_cell_size = w.value("gridCellSize", cfg.world.grid_cell_size);
    }

    // Food
    if (j.contains("food")) {
        const auto& f = j["food"];
        cfg.world.food_spawn_rate = f.value("spawnRate", cfg.world.food_spawn_rate);
        cfg.world.food_max = f.value("max", cfg.world.food_max);
        cfg.world.food_eat_radius = f.value("eatRadius", cfg.world.food_eat_radius);
        cfg.world.food_energy = f.value("energy", cfg.world.food_energy);
    }

    // Energy
    if (j.contains("energy")) {
        const auto& e = j["energy"];
        cfg.world.metabolism_rate = e.value("metabolismRate", cfg.world.metabolism_rate);
        cfg.world.thrust_cost = e.value("thrustCost", cfg.world.thrust_cost);
    }

    // Evolution run parameters
    if (j.contains("evolution")) {
        const auto& ev = j["evolution"];
        cfg.neat.population_size = ev.value("populationSize", cfg.neat.population_size);
        cfg.ticks_per_generation = ev.value("ticksPerGeneration", cfg.ticks_per_generation);
        cfg.generations = ev.value("generations", cfg.generations);
        cfg.save_interval = ev.value("saveInterval", cfg.save_interval);
    }

    // NEAT parameters
    if (j.contains("neat")) {
        const auto& n = j["neat"];
        cfg.neat.add_node_prob = n.value("addNodeProb", cfg.neat.add_node_prob);
        cfg.neat.add_connection_prob = n.value("addConnectionProb", cfg.neat.add_connection_prob);
        cfg.neat.weight_mutate_prob = n.value("weightMutateProb", cfg.neat.weight_mutate_prob);
        cfg.neat.weight_sigma = n.value("weightSigma", cfg.neat.weight_sigma);
        cfg.neat.weight_replace_prob = n.value("weightReplaceProb", cfg.neat.weight_replace_prob);
        cfg.neat.crossover_prob = n.value("crossoverProb", cfg.neat.crossover_prob);
        cfg.neat.compat_threshold = n.value("compatThreshold", cfg.neat.compat_threshold);
        cfg.neat.survival_rate = n.value("survivalRate", cfg.neat.survival_rate);
        cfg.neat.elitism = n.value("elitism", cfg.neat.elitism);
        cfg.neat.max_stagnation = n.value("maxStagnation", cfg.neat.max_stagnation);
    }

    return cfg;
}
