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
        cfg.world.max_speed = w.value("maxSpeed", cfg.world.max_speed);
        cfg.world.max_angular_speed = w.value("maxAngularSpeed", cfg.world.max_angular_speed);
    }

    // Food
    if (j.contains("food")) {
        const auto& f = j["food"];
        cfg.world.food_eat_radius = f.value("eatRadius", cfg.world.food_eat_radius);

        std::string mode = f.value("mode", std::string("uniform"));
        if (mode == "patches") {
            PatchFoodConfig pc;
            pc.num_patches = f.value("numPatches", pc.num_patches);
            pc.food_per_patch = f.value("foodPerPatch", pc.food_per_patch);
            pc.patch_radius = f.value("patchRadius", pc.patch_radius);
            pc.energy = f.value("energy", pc.energy);
            cfg.world.food_source_config = pc;
        } else {
            UniformFoodConfig uc;
            uc.spawn_rate = f.value("spawnRate", uc.spawn_rate);
            uc.max_food = f.value("max", uc.max_food);
            uc.energy = f.value("energy", uc.energy);
            cfg.world.food_source_config = uc;
            // Also populate flat fields for backward compat
            cfg.world.food_spawn_rate = uc.spawn_rate;
            cfg.world.food_max = uc.max_food;
            cfg.world.food_energy = uc.energy;
        }
    }

    // Predator
    if (j.contains("predator")) {
        const auto& p = j["predator"];
        cfg.world.predator_catch_radius = p.value("catchRadius", cfg.world.predator_catch_radius);
        cfg.world.predator_catch_energy = p.value("catchEnergy", cfg.world.predator_catch_energy);
    }

    // Directional mouth
    if (j.contains("mouth")) {
        const auto& m = j["mouth"];
        cfg.world.mouth_enabled = m.value("enabled", cfg.world.mouth_enabled);
        if (m.contains("arcWidth"))
            cfg.world.mouth_arc_width = m["arcWidth"].get<float>() * (3.14159265f / 180.0f);
        cfg.world.mouth_require_approach = m.value("requireApproach", cfg.world.mouth_require_approach);
    }

    // Sensor channels
    if (j.contains("sensors")) {
        const auto& s = j["sensors"];
        if (s.contains("channels")) {
            cfg.world.enabled_channels.clear();
            for (const auto& ch : s["channels"]) {
                std::string name = ch.get<std::string>();
                if (name == "food") cfg.world.enabled_channels.push_back(SensorChannel::Food);
                else if (name == "same") cfg.world.enabled_channels.push_back(SensorChannel::Same);
                else if (name == "opposite") cfg.world.enabled_channels.push_back(SensorChannel::Opposite);
            }
        }
    }

    // Shoaling
    if (j.contains("shoaling")) {
        const auto& sh = j["shoaling"];
        auto load_shoaling = [](const json& obj, WorldConfig::ShoalingConfig& sc) {
            sc.radius = obj.value("radius", sc.radius);
            sc.max_reduction = obj.value("maxReduction", sc.max_reduction);
            sc.max_neighbours = obj.value("maxNeighbours", sc.max_neighbours);
            if (obj.contains("arcDeg"))
                sc.arc = obj["arcDeg"].get<float>() * (3.14159265f / 180.0f);
        };
        if (sh.contains("prey")) load_shoaling(sh["prey"], cfg.world.prey_shoaling);
        if (sh.contains("predator")) load_shoaling(sh["predator"], cfg.world.predator_shoaling);
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
        std::string fm = ev.value("fitnessMode", std::string("gross"));
        if (fm == "net") cfg.fitness_mode = FitnessMode::Net;
        else cfg.fitness_mode = FitnessMode::Gross;
    }

    // Morphology evolution
    if (j.contains("morphologyEvolution")) {
        const auto& me = j["morphologyEvolution"];
        cfg.morphology.enabled = me.value("enabled", cfg.morphology.enabled);

        if (me.contains("groups")) {
            cfg.morphology.groups.clear();
            for (const auto& jg : me["groups"]) {
                MorphologyGroupConfig gc;
                gc.eye_count = jg.value("eyeCount", gc.eye_count);
                gc.total_arc_deg = jg.value("totalArcDeg", gc.total_arc_deg);
                gc.max_range = jg.value("maxRange", gc.max_range);
                cfg.morphology.groups.push_back(gc);
            }
        }

        if (me.contains("mutation")) {
            const auto& mm = me["mutation"];
            cfg.morphology.mutation.angle_sigma_deg = mm.value("angleSigmaDeg", cfg.morphology.mutation.angle_sigma_deg);
            cfg.morphology.mutation.arc_frac_sigma = mm.value("arcFracSigma", cfg.morphology.mutation.arc_frac_sigma);
            cfg.morphology.mutation.angle_mutate_prob = mm.value("angleMutateProb", cfg.morphology.mutation.angle_mutate_prob);
            cfg.morphology.mutation.arc_mutate_prob = mm.value("arcMutateProb", cfg.morphology.mutation.arc_mutate_prob);
            cfg.morphology.mutation.replace_prob = mm.value("replaceProb", cfg.morphology.mutation.replace_prob);
            cfg.morphology.mutation.min_arc_frac = mm.value("minArcFrac", cfg.morphology.mutation.min_arc_frac);
        }
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
        cfg.neat.allow_recurrent = n.value("recurrent", cfg.neat.allow_recurrent);
    }

    return cfg;
}
