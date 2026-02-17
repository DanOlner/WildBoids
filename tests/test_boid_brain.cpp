#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "io/boid_spec.h"
#include "brain/neat_genome.h"
#include "brain/neat_network.h"
#include "simulation/world.h"
#include <filesystem>
#include <cmath>

using Catch::Matchers::WithinAbs;

static std::string data_path(const std::string& filename) {
    for (const auto& prefix : {"data/", "../data/", "../../data/"}) {
        std::string path = std::string(prefix) + filename;
        if (std::filesystem::exists(path)) return path;
    }
    const char* env = std::getenv("WILDBOIDS_DATA_DIR");
    if (env) return std::string(env) + "/" + filename;
    return "data/" + filename;
}

TEST_CASE("Brain-driven boid: minimal genome gives sigmoid(0) thruster power", "[boid_brain]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));

    // Attach a minimal genome (10 sensors → 4 thrusters) with all-zero weights
    int next_innov = 1;
    spec.genome = NeatGenome::minimal(10, 4, next_innov);

    Boid boid = create_boid_from_spec(spec);
    REQUIRE(boid.brain != nullptr);

    // Run the brain manually with zero sensor outputs
    std::vector<float> commands(4);
    boid.brain->activate(boid.sensor_outputs.data(), 10, commands.data(), 4);

    // All-zero weights + zero inputs → sigmoid(0) = 0.5 for all outputs
    for (int i = 0; i < 4; ++i) {
        CHECK_THAT(commands[i], WithinAbs(0.5f, 1e-4f));
    }
}

TEST_CASE("Brain-driven boid: world step activates brain", "[boid_brain]") {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.linear_drag = 0;
    config.angular_drag = 0;
    World world(config);

    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));
    int next_innov = 1;
    spec.genome = NeatGenome::minimal(10, 4, next_innov);

    Boid boid = create_boid_from_spec(spec);
    boid.body.position = {400, 400};
    world.add_boid(std::move(boid));

    // Step once: physics runs (no thrust yet), then sensors, then brain sets powers
    world.step(0.01f);

    const auto& b = world.get_boids()[0];
    // Brain should have set all thrusters to sigmoid(0) = 0.5
    for (const auto& t : b.thrusters) {
        CHECK_THAT(t.power, WithinAbs(0.5f, 1e-3f));
    }

    // With equal thrusters, all at sigmoid(0)=0.5, forces cancel → no net movement.
    // Give rear thruster a bias so it dominates.
    // (This test just verifies the brain activates and physics responds.)
    // We already checked thruster powers are set; that's the key assertion.
}

TEST_CASE("Boid without brain: thrusters unchanged by world step", "[boid_brain]") {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    World world(config);

    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));
    // No genome → no brain

    Boid boid = create_boid_from_spec(spec);
    boid.body.position = {400, 400};
    CHECK(boid.brain == nullptr);

    // Set a specific thruster power
    boid.thrusters[0].power = 0.7f;
    world.add_boid(std::move(boid));

    world.step(0.01f);

    // Brain didn't overwrite — power stays at what was set externally
    CHECK_THAT(world.get_boids()[0].thrusters[0].power, WithinAbs(0.7f, 1e-6f));
}

TEST_CASE("Brain responds to sensor input: boid ahead drives thruster output", "[boid_brain]") {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = false;
    config.linear_drag = 0;
    config.angular_drag = 0;
    World world(config);

    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));
    int next_innov = 1;
    NeatGenome genome = NeatGenome::minimal(10, 4, next_innov);

    // Set a large weight from sensor 0 (forward) to thruster 0 (rear)
    // Sensor 0 → output node 10 (first output, maps to thruster 0)
    // Connection from input 0 to output 10 is connections[0] in minimal topology
    genome.connections[0].weight = 5.0f;
    spec.genome = genome;

    // Boid A at (400, 350) facing forward (+Y)
    Boid boid_a = create_boid_from_spec(spec);
    boid_a.body.position = {400, 350};
    boid_a.body.angle = 0;

    // Boid B at (400, 400) — directly ahead of A
    Boid boid_b = create_boid_from_spec(spec);
    boid_b.body.position = {400, 400};

    world.add_boid(std::move(boid_a));
    world.add_boid(std::move(boid_b));

    // Step to populate grid + sensors + brains
    world.step(0.01f);

    const auto& a = world.get_boids()[0];

    // Sensor 0 (forward) should detect boid B → non-zero signal
    CHECK(a.sensor_outputs[0] > 0.0f);

    // With large positive weight on sensor0→thruster0,
    // thruster 0 should be significantly above 0.5
    CHECK(a.thrusters[0].power > 0.6f);
}

TEST_CASE("Full pipeline: brain-driven boid accelerates forward", "[boid_brain]") {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = false;
    config.linear_drag = 0;
    config.angular_drag = 0;
    World world(config);

    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));
    int next_innov = 1;
    NeatGenome genome = NeatGenome::minimal(10, 4, next_innov);

    // Zero all weights, then give output node 10 (rear thruster) a large positive bias
    // This makes the rear thruster fire regardless of sensor input
    genome.nodes[10].bias = 5.0f;
    // Give front thruster (output node 13) a large negative bias → power near 0
    genome.nodes[13].bias = -5.0f;
    spec.genome = genome;

    Boid boid = create_boid_from_spec(spec);
    boid.body.position = {400, 400};
    boid.body.angle = 0;  // facing +Y
    world.add_boid(std::move(boid));

    // Run for several steps
    for (int i = 0; i < 50; ++i) {
        world.step(0.01f);
    }

    const auto& b = world.get_boids()[0];

    // Rear thruster should be close to sigmoid(5) ≈ 0.993
    CHECK(b.thrusters[0].power > 0.9f);
    // Front thruster should be close to sigmoid(-5) ≈ 0.007
    CHECK(b.thrusters[3].power < 0.1f);

    // Boid should have moved forward (+Y)
    CHECK(b.body.position.y > 400);
    CHECK(b.body.velocity.y > 0);
}

TEST_CASE("Brain-driven boid: spec with genome creates brain automatically", "[boid_brain]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));

    // Without genome → no brain
    Boid boid_no_brain = create_boid_from_spec(spec);
    CHECK(boid_no_brain.brain == nullptr);

    // With genome → brain created
    int next_innov = 1;
    spec.genome = NeatGenome::minimal(10, 4, next_innov);
    Boid boid_with_brain = create_boid_from_spec(spec);
    CHECK(boid_with_brain.brain != nullptr);
}
