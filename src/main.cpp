#include "brain/neat_genome.h"
#include "display/app.h"
#include "display/renderer.h"
#include "io/boid_spec.h"
#include "io/sim_config.h"
#include "simulation/morphology_genome.h"
#include "simulation/world.h"
#include <cstring>
#include <iostream>
#include <random>

int main(int argc, char* argv[]) {
  // Parse args
  std::string prey_champion_path;
  std::string predator_champion_path;
  std::string config_path = "data/sim_config.json";
  int num_boids = 30;
  int num_predators = 0;
  int window_size = 0;  // 0 = use world size
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--champion") == 0 && i + 1 < argc) {
      prey_champion_path = argv[++i];
    } else if (std::strcmp(argv[i], "--prey-champion") == 0 && i + 1 < argc) {
      prey_champion_path = argv[++i];
    } else if (std::strcmp(argv[i], "--predator-champion") == 0 && i + 1 < argc) {
      predator_champion_path = argv[++i];
    } else if (std::strcmp(argv[i], "--boids") == 0 && i + 1 < argc) {
      num_boids = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--predators") == 0 && i + 1 < argc) {
      num_predators = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      config_path = argv[++i];
    } else if (std::strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
      window_size = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::cerr << "Usage: " << argv[0] << " [options]\n"
                << "  --champion PATH          Load evolved prey champion JSON\n"
                << "  --prey-champion PATH     Same as --champion\n"
                << "  --predator-champion PATH Load evolved predator champion JSON\n"
                << "  --boids N                Number of prey boids (default: 30)\n"
                << "  --predators N            Number of predator boids (default: 0)\n"
                << "  --config PATH            Sim config JSON (default: data/sim_config.json)\n"
                << "  --window-size N          Window size in pixels (default: world size)\n"
                << "  --help                   Show this help\n";
      return 0;
    }
  }

  // Load world config from shared sim_config.json
  SimConfig sim;
  try {
    sim = load_sim_config(config_path);
    std::cerr << "Loaded config from: " << config_path << "\n";
  } catch (const std::exception& e) {
    std::cerr << "Failed to load sim config: " << e.what() << "\n";
    return 1;
  }

  World world(sim.world);

  // Load prey boid spec — either from champion or default
  BoidSpec prey_spec;
  try {
    if (!prey_champion_path.empty()) {
      prey_spec = load_boid_spec(prey_champion_path);
      std::cerr << "Loaded prey champion from: " << prey_champion_path << "\n";
      if (!prey_spec.genome.has_value()) {
        std::cerr << "Warning: prey champion file has no genome, boids will have no brain\n";
      }
    } else {
      prey_spec = load_boid_spec("data/simple_boid.json");
    }
  } catch (const std::exception &e) {
    std::cerr << "Failed to load prey boid spec: " << e.what() << "\n";
    std::cerr << "Make sure to run from the project root directory.\n";
    return 1;
  }

  // Load predator boid spec if predators requested
  BoidSpec predator_spec;
  if (num_predators > 0 || !predator_champion_path.empty()) {
    if (num_predators == 0) num_predators = 10;  // default if champion given but count not
    try {
      if (!predator_champion_path.empty()) {
        predator_spec = load_boid_spec(predator_champion_path);
        std::cerr << "Loaded predator champion from: " << predator_champion_path << "\n";
        if (!predator_spec.genome.has_value()) {
          std::cerr << "Warning: predator champion file has no genome\n";
        }
      } else {
        predator_spec = load_boid_spec("data/simple_predator.json");
      }
    } catch (const std::exception &e) {
      std::cerr << "Failed to load predator boid spec: " << e.what() << "\n";
      return 1;
    }
  }

  // Validate morphology config matches boid spec eye counts
  auto validate_morpho = [&](const BoidSpec& spec, const char* label) -> bool {
    if (spec.morphology_genome.has_value() && spec.compound_eyes.has_value()
        && sim.morphology.enabled) {
      std::string err = validate_morphology_config(*spec.compound_eyes, sim.morphology);
      if (!err.empty()) {
        std::cerr << label << " morphology config error: " << err << "\n";
        return false;
      }
    }
    return true;
  };
  if (!validate_morpho(prey_spec, "Prey")) return 1;
  if (num_predators > 0 && !validate_morpho(predator_spec, "Predator")) return 1;

  // Apply morphology genomes to specs if present
  auto apply_morpho = [&](BoidSpec& spec, const char* label) {
    if (spec.morphology_genome.has_value() && spec.compound_eyes.has_value()
        && sim.morphology.enabled) {
      spec.compound_eyes = apply_morphology(
          *spec.compound_eyes, *spec.morphology_genome, sim.morphology);
      std::cerr << "Applied evolved morphology to " << label << " eye layout\n";
    }
  };
  apply_morpho(prey_spec, "prey");
  if (num_predators > 0) apply_morpho(predator_spec, "predator");

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> pos_x(0, sim.world.width);
  std::uniform_real_distribution<float> pos_y(0, sim.world.height);
  std::uniform_real_distribution<float> angle_dist(0, 2.0f * 3.14159265f);

  // Spawn prey boids
  if (!prey_champion_path.empty()) {
    // Champion mode: all prey use the loaded genome
    for (int i = 0; i < num_boids; i++) {
      Boid boid = create_boid_from_spec(prey_spec);
      boid.body.position = {pos_x(rng), pos_y(rng)};
      boid.body.angle = angle_dist(rng);
      world.add_boid(std::move(boid));
    }
  } else {
    // Default mode: random NEAT weights
    std::normal_distribution<float> weight_dist(0.0f, 1.0f);
    int n_sensors = sensor_input_count(prey_spec);
    int n_thrusters = static_cast<int>(prey_spec.thrusters.size());

    int next_innov = 1;
    for (int i = 0; i < num_boids; i++) {
      NeatGenome genome = NeatGenome::minimal(n_sensors, n_thrusters, next_innov);
      next_innov = 1;
      for (auto &c : genome.connections) {
        c.weight = weight_dist(rng);
      }
      prey_spec.genome = genome;

      Boid boid = create_boid_from_spec(prey_spec);
      boid.body.position = {pos_x(rng), pos_y(rng)};
      boid.body.angle = angle_dist(rng);
      world.add_boid(std::move(boid));
    }
  }

  // Spawn predator boids
  if (num_predators > 0) {
    if (!predator_champion_path.empty()) {
      // Champion mode: all predators use the loaded genome
      for (int i = 0; i < num_predators; i++) {
        Boid boid = create_boid_from_spec(predator_spec);
        boid.body.position = {pos_x(rng), pos_y(rng)};
        boid.body.angle = angle_dist(rng);
        world.add_boid(std::move(boid));
      }
    } else {
      // Random NEAT weights for predators
      std::normal_distribution<float> weight_dist(0.0f, 1.0f);
      int n_sensors = sensor_input_count(predator_spec);
      int n_thrusters = static_cast<int>(predator_spec.thrusters.size());

      int next_innov = 1;
      for (int i = 0; i < num_predators; i++) {
        NeatGenome genome = NeatGenome::minimal(n_sensors, n_thrusters, next_innov);
        next_innov = 1;
        for (auto &c : genome.connections) {
          c.weight = weight_dist(rng);
        }
        predator_spec.genome = genome;

        Boid boid = create_boid_from_spec(predator_spec);
        boid.body.position = {pos_x(rng), pos_y(rng)};
        boid.body.angle = angle_dist(rng);
        world.add_boid(std::move(boid));
      }
    }
    std::cerr << "Spawned " << num_predators << " predators\n";
  }

  // Create window and run
  try {
    int window_w = window_size > 0 ? window_size : static_cast<int>(sim.world.width);
    int window_h = window_size > 0 ? window_size : static_cast<int>(sim.world.height);
    Renderer renderer(window_w, window_h, "Wild Boids");
    App app(world, renderer, rng);
    app.run();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
