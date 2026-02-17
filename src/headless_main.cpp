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

    std::uniform_real_distribution<float> x_dist(0.0f, config.width);
    std::uniform_real_distribution<float> y_dist(0.0f, config.height);
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * static_cast<float>(M_PI));

    // Pre-seed some food
    for (int i = 0; i < config.food_max / 2; ++i) {
        world.add_food(Food{Vec2{x_dist(rng), y_dist(rng)}, config.food_energy});
    }

    // Spawn one boid per genome
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

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "\n  Config:\n"
              << "  --config PATH      Sim config JSON (default: data/sim_config.json)\n"
              << "  --spec PATH        Boid spec JSON (default: data/simple_boid.json)\n"
              << "\n  Evolution (override config):\n"
              << "  --generations N    Number of generations\n"
              << "  --seed N           RNG seed (default: 42)\n"
              << "  --population N     Population size\n"
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
    std::string output_dir = "data/champions";
    bool save_best = false;

    // Track which CLI flags were explicitly set (to override config)
    bool cli_generations = false, cli_population = false, cli_ticks = false;
    bool cli_save_interval = false;
    bool cli_world_size = false, cli_food_max = false, cli_food_rate = false;
    bool cli_food_energy = false, cli_metabolism = false, cli_thrust_cost = false;
    bool cli_angular_drag = false, cli_linear_drag = false;

    // Temp storage for CLI overrides
    int ov_generations = 0, ov_population = 0, ov_ticks = 0, ov_save_interval = 0;
    int ov_food_max = 0;
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
        } else if (std::strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--generations") == 0 && i + 1 < argc) {
            ov_generations = std::atoi(argv[++i]); cli_generations = true;
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            rng_seed = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--population") == 0 && i + 1 < argc) {
            ov_population = std::atoi(argv[++i]); cli_population = true;
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

    // Load boid spec
    BoidSpec spec;
    try {
        spec = load_boid_spec(spec_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load boid spec: " << e.what() << "\n";
        return 1;
    }

    int n_sensors = static_cast<int>(spec.sensors.size());
    int n_thrusters = static_cast<int>(spec.thrusters.size());

    // Create seed genome and population
    std::mt19937 rng(static_cast<unsigned>(rng_seed));
    int next_innov = 1;
    NeatGenome seed = NeatGenome::minimal(n_sensors, n_thrusters, next_innov);

    Population pop(seed, sim.neat, rng);

    // Print config summary to stderr
    std::cerr << "World: " << sim.world.width << "x" << sim.world.height
              << "  Pop: " << sim.neat.population_size
              << "  Ticks/gen: " << sim.ticks_per_generation
              << "  Food: " << sim.world.food_max << " max, " << sim.world.food_spawn_rate << "/s"
              << "  Metabolism: " << sim.world.metabolism_rate
              << "  Thrust cost: " << sim.world.thrust_cost
              << "  Drag: " << sim.world.linear_drag << "/" << sim.world.angular_drag << "\n";

    // Print header
    std::cout << "gen,best_fitness,mean_fitness,species_count,pop_size\n";

    // Create output directory
    std::filesystem::create_directories(output_dir);

    float all_time_best = 0.0f;
    int all_time_best_gen = -1;

    // Evolution loop
    for (int gen = 0; gen < sim.generations; ++gen) {
        auto fitness = run_generation(pop.genomes(), spec, sim.world, sim.ticks_per_generation, rng);

        pop.evaluate([&](int idx, const NeatGenome&) {
            return fitness[idx];
        });

        float mean_fitness = 0.0f;
        for (float f : fitness) mean_fitness += f;
        mean_fitness /= static_cast<float>(fitness.size());

        std::cout << gen << ","
                  << pop.best_fitness() << ","
                  << mean_fitness << ","
                  << pop.species_count() << ","
                  << pop.size() << "\n";

        // Track all-time best
        bool is_new_best = pop.best_fitness() > all_time_best;
        if (is_new_best) {
            all_time_best = pop.best_fitness();
            all_time_best_gen = gen;
            std::cerr << "  New all-time best: " << all_time_best
                      << " at gen " << gen << "\n";
        }

        // Save champion periodically
        bool interval_save = sim.save_interval > 0 &&
            (gen % sim.save_interval == 0 || gen == sim.generations - 1);
        bool best_save = save_best && is_new_best;

        if (interval_save || best_save) {
            BoidSpec champion_spec = spec;
            champion_spec.genome = pop.best_genome();
            std::string path = output_dir + "/champion_gen" + std::to_string(gen) + ".json";
            try {
                save_boid_spec(champion_spec, path);
            } catch (const std::exception& e) {
                std::cerr << "Warning: failed to save champion: " << e.what() << "\n";
            }
        }

        if (gen < sim.generations - 1) {
            pop.advance_generation();
        }
    }

    // Final summary
    std::cerr << "\nEvolution complete.\n"
              << "  Generations: " << sim.generations << "\n"
              << "  All-time best fitness: " << all_time_best
              << " (gen " << all_time_best_gen << ")\n"
              << "  Best champion file: " << output_dir << "/champion_gen"
              << all_time_best_gen << ".json\n"
              << "  Final gen species: " << pop.species_count() << "\n";

    return 0;
}
