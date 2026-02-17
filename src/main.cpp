#include "simulation/world.h"
#include "io/boid_spec.h"
#include "brain/neat_genome.h"
#include "display/renderer.h"
#include "display/app.h"
#include <random>
#include <iostream>

int main() {
    WorldConfig config;
    config.width = 800.0f;
    config.height = 800.0f;
    config.toroidal = true;
    config.linear_drag = 0.02f;
    config.angular_drag = 0.05f;

    // Food and energy
    config.food_spawn_rate = 3.0f;
    config.food_max = 60;
    config.food_eat_radius = 10.0f;
    config.food_energy = 20.0f;
    config.metabolism_rate = 0.3f;
    config.thrust_cost = 0.02f;

    World world(config);

    // Load boid spec
    BoidSpec spec;
    try {
        spec = load_boid_spec("data/simple_boid.json");
    } catch (const std::exception& e) {
        std::cerr << "Failed to load boid spec: " << e.what() << "\n";
        std::cerr << "Make sure to run from the project root directory.\n";
        return 1;
    }

    // Spawn boids at random positions with random headings.
    // Half get a NEAT brain with random weights, half use random wander.
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos_x(0, config.width);
    std::uniform_real_distribution<float> pos_y(0, config.height);
    std::uniform_real_distribution<float> angle_dist(0, 2.0f * 3.14159265f);
    std::normal_distribution<float> weight_dist(0.0f, 1.0f);

    int n_sensors = static_cast<int>(spec.sensors.size());
    int n_thrusters = static_cast<int>(spec.thrusters.size());

    int next_innov = 1;
    for (int i = 0; i < 30; i++) {
        NeatGenome genome = NeatGenome::minimal(n_sensors, n_thrusters, next_innov);
        // Reset innovation counter so all brains share the same topology
        next_innov = 1;
        for (auto& c : genome.connections) {
            c.weight = weight_dist(rng);
        }
        spec.genome = genome;

        Boid boid = create_boid_from_spec(spec);
        boid.body.position = {pos_x(rng), pos_y(rng)};
        boid.body.angle = angle_dist(rng);
        world.add_boid(std::move(boid));
    }

    // Create window and run
    try {
        Renderer renderer(800, 800, "Wild Boids");
        App app(world, renderer, rng);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
