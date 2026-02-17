#include "brain/neat_genome.h"
#include "display/app.h"
#include "display/renderer.h"
#include "io/boid_spec.h"
#include "io/sim_config.h"
#include "simulation/world.h"
#include <cstring>
#include <iostream>
#include <random>

int main(int argc, char* argv[]) {
  // Parse args
  std::string champion_path;
  std::string config_path = "data/sim_config.json";
  int num_boids = 30;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--champion") == 0 && i + 1 < argc) {
      champion_path = argv[++i];
    } else if (std::strcmp(argv[i], "--boids") == 0 && i + 1 < argc) {
      num_boids = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      config_path = argv[++i];
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::cerr << "Usage: " << argv[0] << " [options]\n"
                << "  --champion PATH  Load evolved champion JSON (all boids use this brain)\n"
                << "  --boids N        Number of boids to spawn (default: 30)\n"
                << "  --config PATH    Sim config JSON (default: data/sim_config.json)\n"
                << "  --help           Show this help\n";
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

  // Load boid spec — either from champion or default
  BoidSpec spec;
  try {
    if (!champion_path.empty()) {
      spec = load_boid_spec(champion_path);
      std::cerr << "Loaded champion from: " << champion_path << "\n";
      if (!spec.genome.has_value()) {
        std::cerr << "Warning: champion file has no genome, boids will have no brain\n";
      }
    } else {
      spec = load_boid_spec("data/simple_boid.json");
    }
  } catch (const std::exception &e) {
    std::cerr << "Failed to load boid spec: " << e.what() << "\n";
    std::cerr << "Make sure to run from the project root directory.\n";
    return 1;
  }

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> pos_x(0, sim.world.width);
  std::uniform_real_distribution<float> pos_y(0, sim.world.height);
  std::uniform_real_distribution<float> angle_dist(0, 2.0f * 3.14159265f);

  if (!champion_path.empty()) {
    // Champion mode: all boids use the loaded genome
    for (int i = 0; i < num_boids; i++) {
      Boid boid = create_boid_from_spec(spec);
      boid.body.position = {pos_x(rng), pos_y(rng)};
      boid.body.angle = angle_dist(rng);
      world.add_boid(std::move(boid));
    }
  } else {
    // Default mode: random NEAT weights
    std::normal_distribution<float> weight_dist(0.0f, 1.0f);
    int n_sensors = static_cast<int>(spec.sensors.size());
    int n_thrusters = static_cast<int>(spec.thrusters.size());

    int next_innov = 1;
    for (int i = 0; i < num_boids; i++) {
      NeatGenome genome = NeatGenome::minimal(n_sensors, n_thrusters, next_innov);
      next_innov = 1;
      for (auto &c : genome.connections) {
        c.weight = weight_dist(rng);
      }
      spec.genome = genome;

      Boid boid = create_boid_from_spec(spec);
      boid.body.position = {pos_x(rng), pos_y(rng)};
      boid.body.angle = angle_dist(rng);
      world.add_boid(std::move(boid));
    }
  }

  // Create window and run
  try {
    int window_w = static_cast<int>(sim.world.width);
    int window_h = static_cast<int>(sim.world.height);
    Renderer renderer(window_w, window_h, "Wild Boids");
    App app(world, renderer, rng);
    app.run();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
