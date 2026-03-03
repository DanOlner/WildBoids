#include "brain/neat_genome.h"
#include "brain/neat_network.h"
#include "brain/population.h"
#include "io/boid_spec.h"
#include "io/sim_config.h"
#include "simulation/world.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <random>

struct GenerationResult {
    std::vector<float> prey_fitness;
    std::vector<float> predator_fitness;
    int prey_survivors = 0;
    int predator_survivors = 0;
};

// Run one generation: create a World, spawn prey and predator boids with
// genomes as brains, run for N ticks, return fitness for each genome.
// Prey are spawned at indices [0, prey_count), predators at [prey_count, total).
static GenerationResult run_generation(
    const std::vector<NeatGenome>& prey_genomes,
    const std::vector<NeatGenome>& predator_genomes,
    const BoidSpec& prey_spec,
    const BoidSpec& predator_spec,
    const WorldConfig& config,
    int ticks,
    FitnessMode fitness_mode,
    std::mt19937& rng)
{
    World world(config);

    std::uniform_real_distribution<float> x_dist(0.0f, config.width);
    std::uniform_real_distribution<float> y_dist(0.0f, config.height);
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * static_cast<float>(M_PI));

    // Pre-seed food using the configured food source strategy
    world.pre_seed_food(rng);

    // Spawn prey boids at indices [0, prey_count)
    for (const auto& genome : prey_genomes) {
        Boid boid = create_boid_from_spec(prey_spec);
        boid.body.position = Vec2{x_dist(rng), y_dist(rng)};
        boid.body.angle = angle_dist(rng);
        boid.brain = std::make_unique<NeatNetwork>(genome);
        world.add_boid(std::move(boid));
    }

    // Spawn predator boids at indices [prey_count, prey_count + predator_count)
    for (const auto& genome : predator_genomes) {
        Boid boid = create_boid_from_spec(predator_spec);
        boid.body.position = Vec2{x_dist(rng), y_dist(rng)};
        boid.body.angle = angle_dist(rng);
        boid.brain = std::make_unique<NeatNetwork>(genome);
        world.add_boid(std::move(boid));
    }

    int prey_count = static_cast<int>(prey_genomes.size());

    // Run simulation
    float dt = 1.0f / 120.0f;
    for (int t = 0; t < ticks; ++t) {
        world.step(dt, &rng);

        // Early exit if all prey are dead (predators can't do anything without prey)
        const auto& boids = world.get_boids();
        bool any_prey_alive = false;
        for (int i = 0; i < prey_count; ++i) {
            if (boids[i].alive) { any_prey_alive = true; break; }
        }
        if (!any_prey_alive) break;
    }

    // Collect fitness and count survivors
    GenerationResult result;
    result.prey_fitness.resize(prey_genomes.size());
    result.predator_fitness.resize(predator_genomes.size());

    auto boid_fitness = [fitness_mode](const Boid& b) -> float {
        if (fitness_mode == FitnessMode::Net)
            return b.total_energy_gained - b.total_energy_spent;
        return b.total_energy_gained;
    };

    const auto& boids = world.get_boids();
    for (int i = 0; i < static_cast<int>(prey_genomes.size()); ++i) {
        result.prey_fitness[i] = boid_fitness(boids[i]);
        if (boids[i].alive) ++result.prey_survivors;
    }
    for (int i = 0; i < static_cast<int>(predator_genomes.size()); ++i) {
        result.predator_fitness[i] = boid_fitness(boids[prey_count + i]);
        if (boids[prey_count + i].alive) ++result.predator_survivors;
    }

    return result;
}

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "\n  Config:\n"
              << "  --config PATH      Sim config JSON (default: data/sim_config.json)\n"
              << "  --spec PATH        Prey boid spec JSON (default: data/simple_boid.json)\n"
              << "  --predator-spec PATH  Predator boid spec (enables co-evolution)\n"
              << "\n  Evolution (override config):\n"
              << "  --generations N    Number of generations\n"
              << "  --seed N           RNG seed (default: 42)\n"
              << "  --population N     Prey population size\n"
              << "  --predator-population N  Predator population size (default: same as prey)\n"
              << "  --ticks N          Ticks per generation\n"
              << "\n  World (override config):\n"
              << "  --world-size N     World width and height\n"
              << "  --food-max N       Max food items\n"
              << "  --food-rate F      Food spawns per second\n"
              << "  --food-energy F    Energy per food item\n"
              << "  --metabolism F     Energy lost per second\n"
              << "  --thrust-cost F    Energy cost per thrust per second\n"
              << "  --angular-drag F   Angular drag coefficient\n"
              << "  --linear-drag F    Linear drag coefficient\n"
              << "\n  Output:\n"
              << "  --save-interval N  Save champion every N gens\n"
              << "  --output-dir PATH  Directory for saved genomes (default: data/champions)\n"
              << "  --save-best        Save whenever a new all-time best fitness is found\n"
              << "  --help             Show this help\n";
}

int main(int argc, char* argv[]) {
    // Defaults that can't come from config
    int rng_seed = 42;
    std::string config_path = "data/sim_config.json";
    std::string spec_path = "data/simple_boid.json";
    std::string predator_spec_path;  // empty = no predators (backward compat)
    std::string output_dir = "data/champions";
    bool save_best = false;

    // Track which CLI flags were explicitly set (to override config)
    bool cli_generations = false, cli_population = false, cli_ticks = false;
    bool cli_save_interval = false;
    bool cli_world_size = false, cli_food_max = false, cli_food_rate = false;
    bool cli_food_energy = false, cli_metabolism = false, cli_thrust_cost = false;
    bool cli_angular_drag = false, cli_linear_drag = false;
    bool cli_predator_population = false;

    // Temp storage for CLI overrides
    int ov_generations = 0, ov_population = 0, ov_ticks = 0, ov_save_interval = 0;
    int ov_food_max = 0, ov_predator_population = 0;
    float ov_world_size = 0, ov_food_rate = 0, ov_food_energy = 0;
    float ov_metabolism = 0, ov_thrust_cost = 0;
    float ov_angular_drag = 0, ov_linear_drag = 0;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--save-best") == 0) {
            save_best = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (std::strcmp(argv[i], "--spec") == 0 && i + 1 < argc) {
            spec_path = argv[++i];
        } else if (std::strcmp(argv[i], "--predator-spec") == 0 && i + 1 < argc) {
            predator_spec_path = argv[++i];
        } else if (std::strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--generations") == 0 && i + 1 < argc) {
            ov_generations = std::atoi(argv[++i]); cli_generations = true;
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            rng_seed = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--population") == 0 && i + 1 < argc) {
            ov_population = std::atoi(argv[++i]); cli_population = true;
        } else if (std::strcmp(argv[i], "--predator-population") == 0 && i + 1 < argc) {
            ov_predator_population = std::atoi(argv[++i]); cli_predator_population = true;
        } else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            ov_ticks = std::atoi(argv[++i]); cli_ticks = true;
        } else if (std::strcmp(argv[i], "--save-interval") == 0 && i + 1 < argc) {
            ov_save_interval = std::atoi(argv[++i]); cli_save_interval = true;
        } else if (std::strcmp(argv[i], "--world-size") == 0 && i + 1 < argc) {
            ov_world_size = static_cast<float>(std::atof(argv[++i])); cli_world_size = true;
        } else if (std::strcmp(argv[i], "--food-max") == 0 && i + 1 < argc) {
            ov_food_max = std::atoi(argv[++i]); cli_food_max = true;
        } else if (std::strcmp(argv[i], "--food-rate") == 0 && i + 1 < argc) {
            ov_food_rate = static_cast<float>(std::atof(argv[++i])); cli_food_rate = true;
        } else if (std::strcmp(argv[i], "--food-energy") == 0 && i + 1 < argc) {
            ov_food_energy = static_cast<float>(std::atof(argv[++i])); cli_food_energy = true;
        } else if (std::strcmp(argv[i], "--metabolism") == 0 && i + 1 < argc) {
            ov_metabolism = static_cast<float>(std::atof(argv[++i])); cli_metabolism = true;
        } else if (std::strcmp(argv[i], "--thrust-cost") == 0 && i + 1 < argc) {
            ov_thrust_cost = static_cast<float>(std::atof(argv[++i])); cli_thrust_cost = true;
        } else if (std::strcmp(argv[i], "--angular-drag") == 0 && i + 1 < argc) {
            ov_angular_drag = static_cast<float>(std::atof(argv[++i])); cli_angular_drag = true;
        } else if (std::strcmp(argv[i], "--linear-drag") == 0 && i + 1 < argc) {
            ov_linear_drag = static_cast<float>(std::atof(argv[++i])); cli_linear_drag = true;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Load sim config from JSON
    SimConfig sim;
    try {
        sim = load_sim_config(config_path);
        std::cerr << "Loaded config from: " << config_path << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Failed to load sim config: " << e.what() << "\n";
        return 1;
    }

    // Apply CLI overrides
    if (cli_generations)  sim.generations = ov_generations;
    if (cli_population)   sim.neat.population_size = ov_population;
    if (cli_ticks)        sim.ticks_per_generation = ov_ticks;
    if (cli_save_interval) sim.save_interval = ov_save_interval;
    if (cli_world_size)   { sim.world.width = ov_world_size; sim.world.height = ov_world_size; }
    if (cli_food_max)     sim.world.food_max = ov_food_max;
    if (cli_food_rate)    sim.world.food_spawn_rate = ov_food_rate;
    if (cli_food_energy)  sim.world.food_energy = ov_food_energy;
    if (cli_metabolism)   sim.world.metabolism_rate = ov_metabolism;
    if (cli_thrust_cost)  sim.world.thrust_cost = ov_thrust_cost;
    if (cli_angular_drag) sim.world.angular_drag = ov_angular_drag;
    if (cli_linear_drag)  sim.world.linear_drag = ov_linear_drag;

    bool coevolution = !predator_spec_path.empty();

    // Load prey boid spec
    BoidSpec prey_spec;
    try {
        prey_spec = load_boid_spec(spec_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load prey boid spec: " << e.what() << "\n";
        return 1;
    }

    int prey_n_sensors = sensor_input_count(prey_spec);
    int prey_n_thrusters = static_cast<int>(prey_spec.thrusters.size());

    // Create seed genome and prey population
    // If the spec has an embedded genome (i.e. it's a champion file), use it as the seed
    std::mt19937 rng(static_cast<unsigned>(rng_seed));
    int next_innov = 1;
    NeatGenome prey_seed;
    if (prey_spec.genome.has_value()) {
        prey_seed = *prey_spec.genome;
        std::cerr << "Seeding prey population from champion genome ("
                  << prey_seed.connections.size() << " connections, "
                  << prey_seed.nodes.size() << " nodes)\n";
    } else {
        prey_seed = NeatGenome::minimal(prey_n_sensors, prey_n_thrusters, next_innov);
    }

    Population prey_pop(prey_seed, sim.neat, rng);

    // Load predator spec and create predator population (if co-evolution enabled)
    BoidSpec predator_spec;
    std::unique_ptr<Population> predator_pop;
    PopulationParams predator_neat_params = sim.neat;

    if (coevolution) {
        try {
            predator_spec = load_boid_spec(predator_spec_path);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load predator boid spec: " << e.what() << "\n";
            return 1;
        }

        int pred_n_sensors = sensor_input_count(predator_spec);
        int pred_n_thrusters = static_cast<int>(predator_spec.thrusters.size());

        // Use predator population size if specified, otherwise use same as prey
        if (cli_predator_population) {
            predator_neat_params.population_size = ov_predator_population;
        }

        next_innov = 1;
        NeatGenome pred_seed;
        if (predator_spec.genome.has_value()) {
            pred_seed = *predator_spec.genome;
            std::cerr << "Seeding predator population from champion genome ("
                      << pred_seed.connections.size() << " connections, "
                      << pred_seed.nodes.size() << " nodes)\n";
        } else {
            pred_seed = NeatGenome::minimal(pred_n_sensors, pred_n_thrusters, next_innov);
        }
        predator_pop = std::make_unique<Population>(pred_seed, predator_neat_params, rng);

        std::cerr << "Co-evolution enabled: " << predator_neat_params.population_size
                  << " predators\n";
    }

    // Print config summary to stderr
    std::cerr << "World: " << sim.world.width << "x" << sim.world.height
              << "  Prey pop: " << sim.neat.population_size
              << "  Ticks/gen: " << sim.ticks_per_generation
              << "  Food: " << sim.world.food_max << " max, " << sim.world.food_spawn_rate << "/s"
              << "  Metabolism: " << sim.world.metabolism_rate;
    if (prey_spec.metabolism_rate.has_value())
        std::cerr << " (prey: " << *prey_spec.metabolism_rate << ")";
    if (coevolution && predator_spec.metabolism_rate.has_value())
        std::cerr << " (pred: " << *predator_spec.metabolism_rate << ")";
    std::cerr << "  Thrust cost: " << sim.world.thrust_cost
              << "  Drag: " << sim.world.linear_drag << "/" << sim.world.angular_drag
              << "  Fitness: " << (sim.fitness_mode == FitnessMode::Net ? "net" : "gross") << "\n";

    // Print header
    if (coevolution) {
        std::cout << "gen,prey_best,prey_mean,pred_best,pred_mean,"
                  << "prey_species,pred_species,prey_survivors,pred_survivors\n";
    } else {
        std::cout << "gen,best_fitness,mean_fitness,species_count,pop_size,survivors\n";
    }

    // Create output directories
    std::filesystem::create_directories(output_dir);
    std::string predator_output_dir = output_dir + "/predators";
    if (coevolution) {
        std::filesystem::create_directories(predator_output_dir);
    }

    float prey_all_time_best = 0.0f;
    int prey_all_time_best_gen = -1;
    float pred_all_time_best = 0.0f;
    int pred_all_time_best_gen = -1;

    // Evolution loop
    for (int gen = 0; gen < sim.generations; ++gen) {
        // Empty predator genomes vector for prey-only mode
        static const std::vector<NeatGenome> no_predators;

        auto result = run_generation(
            prey_pop.genomes(),
            coevolution ? predator_pop->genomes() : no_predators,
            prey_spec,
            predator_spec,
            sim.world,
            sim.ticks_per_generation,
            sim.fitness_mode,
            rng);

        // Evaluate prey fitness
        prey_pop.evaluate([&](int idx, const NeatGenome&) {
            return result.prey_fitness[idx];
        });

        float prey_mean = 0.0f;
        for (float f : result.prey_fitness) prey_mean += f;
        prey_mean /= static_cast<float>(result.prey_fitness.size());

        if (coevolution) {
            // Evaluate predator fitness
            predator_pop->evaluate([&](int idx, const NeatGenome&) {
                return result.predator_fitness[idx];
            });

            float pred_mean = 0.0f;
            for (float f : result.predator_fitness) pred_mean += f;
            pred_mean /= static_cast<float>(result.predator_fitness.size());

            std::cout << gen << ","
                      << prey_pop.best_fitness() << ","
                      << prey_mean << ","
                      << predator_pop->best_fitness() << ","
                      << pred_mean << ","
                      << prey_pop.species_count() << ","
                      << predator_pop->species_count() << ","
                      << result.prey_survivors << ","
                      << result.predator_survivors << "\n";

            // Track predator all-time best
            bool pred_new_best = predator_pop->best_fitness() > pred_all_time_best;
            if (pred_new_best) {
                pred_all_time_best = predator_pop->best_fitness();
                pred_all_time_best_gen = gen;
                std::cerr << "  New predator all-time best: " << pred_all_time_best
                          << " at gen " << gen << "\n";
            }
        } else {
            std::cout << gen << ","
                      << prey_pop.best_fitness() << ","
                      << prey_mean << ","
                      << prey_pop.species_count() << ","
                      << prey_pop.size() << ","
                      << result.prey_survivors << "\n";
        }

        // Track prey all-time best
        bool prey_new_best = prey_pop.best_fitness() > prey_all_time_best;
        if (prey_new_best) {
            prey_all_time_best = prey_pop.best_fitness();
            prey_all_time_best_gen = gen;
            std::cerr << "  New prey all-time best: " << prey_all_time_best
                      << " at gen " << gen << "\n";
        }

        // Save champions periodically
        bool interval_save = sim.save_interval > 0 &&
            (gen % sim.save_interval == 0 || gen == sim.generations - 1);
        bool best_prey_save = save_best && prey_new_best;

        if (interval_save || best_prey_save) {
            BoidSpec champion_spec = prey_spec;
            champion_spec.genome = prey_pop.best_genome();
            std::string prefix = coevolution ? "champion_prey_gen" : "champion_gen";
            std::string path = output_dir + "/" + prefix + std::to_string(gen) + ".json";
            try {
                save_boid_spec(champion_spec, path);
            } catch (const std::exception& e) {
                std::cerr << "Warning: failed to save prey champion: " << e.what() << "\n";
            }
        }

        if (coevolution) {
            bool best_pred_save = save_best &&
                (predator_pop->best_fitness() > pred_all_time_best ||
                 (pred_all_time_best_gen == gen));

            if (interval_save || best_pred_save) {
                BoidSpec champion_spec = predator_spec;
                champion_spec.genome = predator_pop->best_genome();
                std::string path = predator_output_dir + "/champion_predator_gen"
                    + std::to_string(gen) + ".json";
                try {
                    save_boid_spec(champion_spec, path);
                } catch (const std::exception& e) {
                    std::cerr << "Warning: failed to save predator champion: " << e.what() << "\n";
                }
            }
        }

        if (gen < sim.generations - 1) {
            prey_pop.advance_generation();
            if (coevolution) {
                predator_pop->advance_generation();
            }
        }
    }

    // Final summary
    std::cerr << "\nEvolution complete.\n"
              << "  Generations: " << sim.generations << "\n"
              << "  Prey all-time best fitness: " << prey_all_time_best
              << " (gen " << prey_all_time_best_gen << ")\n";

    if (coevolution) {
        std::cerr << "  Predator all-time best fitness: " << pred_all_time_best
                  << " (gen " << pred_all_time_best_gen << ")\n"
                  << "  Final prey species: " << prey_pop.species_count() << "\n"
                  << "  Final predator species: " << predator_pop->species_count() << "\n";
    } else {
        std::string prefix = coevolution ? "champion_prey_gen" : "champion_gen";
        std::cerr << "  Best champion file: " << output_dir << "/" << prefix
                  << prey_all_time_best_gen << ".json\n"
                  << "  Final gen species: " << prey_pop.species_count() << "\n";
    }

    return 0;
}
