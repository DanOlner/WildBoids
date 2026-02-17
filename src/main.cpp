#include "simulation/world.h"
#include "io/boid_spec.h"
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

    // Spawn boids at random positions with random headings
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos_x(0, config.width);
    std::uniform_real_distribution<float> pos_y(0, config.height);
    std::uniform_real_distribution<float> angle_dist(0, 2.0f * 3.14159265f);

    for (int i = 0; i < 30; i++) {
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
