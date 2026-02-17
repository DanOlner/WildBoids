#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "io/boid_spec.h"
#include "brain/neat_genome.h"
#include "brain/neat_network.h"
#include <filesystem>

using Catch::Matchers::WithinAbs;

// Tests need to find data/simple_boid.json. CMake runs tests from the build dir,
// so we locate the project root via the source dir set at configure time.
// For now, use a relative path that works when running from the project root.
static std::string data_path(const std::string& filename) {
    // Try a few common locations
    for (const auto& prefix : {"data/", "../data/", "../../data/"}) {
        std::string path = std::string(prefix) + filename;
        if (std::filesystem::exists(path)) return path;
    }
    // Fallback: use WILDBOIDS_DATA_DIR if set via CMake
    const char* env = std::getenv("WILDBOIDS_DATA_DIR");
    if (env) return std::string(env) + "/" + filename;
    return "data/" + filename;
}

TEST_CASE("Load simple_boid.json", "[boid_spec]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));

    CHECK(spec.version == "0.2");
    CHECK(spec.type == "prey");
    CHECK_THAT(spec.mass, WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(spec.moment_of_inertia, WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(spec.initial_energy, WithinAbs(100.0f, 1e-6f));
    REQUIRE(spec.thrusters.size() == 4);

    CHECK(spec.thrusters[0].label == "rear");
    CHECK(spec.thrusters[0].max_thrust > 0);
    CHECK(spec.thrusters[3].label == "front");

    // Sensors
    REQUIRE(spec.sensors.size() == 10);
    CHECK(spec.sensors[0].id == 0);
    CHECK_THAT(spec.sensors[0].center_angle, WithinAbs(0.0f, 1e-4f));
    CHECK(spec.sensors[0].filter == EntityFilter::Any);
    CHECK(spec.sensors[0].signal_type == SignalType::NearestDistance);
}

TEST_CASE("Create boid from spec", "[boid_spec]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));
    Boid boid = create_boid_from_spec(spec);

    CHECK(boid.type == "prey");
    CHECK_THAT(boid.body.mass, WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(boid.body.moment_of_inertia, WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(boid.energy, WithinAbs(100.0f, 1e-6f));
    REQUIRE(boid.thrusters.size() == 4);

    // All thrusters start at zero power
    for (const auto& t : boid.thrusters) {
        CHECK_THAT(t.power, WithinAbs(0.0f, 1e-6f));
    }

    // Sensors wired up
    REQUIRE(boid.sensors.has_value());
    CHECK(boid.sensors->input_count() == 10);
    CHECK(boid.sensor_outputs.size() == 10);
}

TEST_CASE("Round-trip save and reload", "[boid_spec]") {
    BoidSpec original = load_boid_spec(data_path("simple_boid.json"));

    std::string tmp_path = "test_roundtrip_boid.json";
    save_boid_spec(original, tmp_path);

    BoidSpec reloaded = load_boid_spec(tmp_path);

    CHECK(reloaded.version == original.version);
    CHECK(reloaded.type == original.type);
    CHECK_THAT(reloaded.mass, WithinAbs(original.mass, 1e-6f));
    CHECK(reloaded.thrusters.size() == original.thrusters.size());

    for (size_t i = 0; i < original.thrusters.size(); i++) {
        CHECK(reloaded.thrusters[i].label == original.thrusters[i].label);
        CHECK_THAT(reloaded.thrusters[i].max_thrust,
                   WithinAbs(original.thrusters[i].max_thrust, 1e-4f));
    }

    // Sensors round-trip
    REQUIRE(reloaded.sensors.size() == original.sensors.size());
    for (size_t i = 0; i < original.sensors.size(); i++) {
        CHECK(reloaded.sensors[i].id == original.sensors[i].id);
        CHECK_THAT(reloaded.sensors[i].center_angle,
                   WithinAbs(original.sensors[i].center_angle, 1e-3f));
        CHECK_THAT(reloaded.sensors[i].arc_width,
                   WithinAbs(original.sensors[i].arc_width, 1e-3f));
        CHECK_THAT(reloaded.sensors[i].max_range,
                   WithinAbs(original.sensors[i].max_range, 1e-4f));
        CHECK(reloaded.sensors[i].filter == original.sensors[i].filter);
        CHECK(reloaded.sensors[i].signal_type == original.sensors[i].signal_type);
    }

    // Clean up
    std::filesystem::remove(tmp_path);
}

TEST_CASE("Invalid file throws", "[boid_spec]") {
    CHECK_THROWS(load_boid_spec("nonexistent_file.json"));
}

TEST_CASE("Spec without genome loads with no genome", "[boid_spec]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));
    CHECK_FALSE(spec.genome.has_value());
}

TEST_CASE("Genome round-trip: save and reload preserves genome", "[boid_spec]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));

    // Attach a minimal genome (10 sensors → 4 thrusters)
    int next_innov = 1;
    NeatGenome genome = NeatGenome::minimal(10, 4, next_innov);
    // Set some non-zero weights to verify they round-trip
    genome.connections[0].weight = 1.5f;
    genome.connections[3].weight = -0.7f;
    genome.connections[5].enabled = false;
    genome.nodes[10].bias = 0.25f; // first output node
    spec.genome = genome;

    std::string tmp_path = "test_genome_roundtrip.json";
    save_boid_spec(spec, tmp_path);

    BoidSpec reloaded = load_boid_spec(tmp_path);
    REQUIRE(reloaded.genome.has_value());

    const auto& rg = *reloaded.genome;
    REQUIRE(rg.nodes.size() == genome.nodes.size());
    REQUIRE(rg.connections.size() == genome.connections.size());

    for (size_t i = 0; i < genome.nodes.size(); ++i) {
        CHECK(rg.nodes[i].id == genome.nodes[i].id);
        CHECK(rg.nodes[i].type == genome.nodes[i].type);
        CHECK(rg.nodes[i].activation == genome.nodes[i].activation);
        CHECK_THAT(rg.nodes[i].bias, WithinAbs(genome.nodes[i].bias, 1e-4f));
    }

    for (size_t i = 0; i < genome.connections.size(); ++i) {
        CHECK(rg.connections[i].innovation == genome.connections[i].innovation);
        CHECK(rg.connections[i].source == genome.connections[i].source);
        CHECK(rg.connections[i].target == genome.connections[i].target);
        CHECK_THAT(rg.connections[i].weight, WithinAbs(genome.connections[i].weight, 1e-4f));
        CHECK(rg.connections[i].enabled == genome.connections[i].enabled);
    }

    std::filesystem::remove(tmp_path);
}

TEST_CASE("Genome round-trip: network produces identical outputs", "[boid_spec]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));

    int next_innov = 1;
    NeatGenome genome = NeatGenome::minimal(10, 4, next_innov);
    genome.connections[0].weight = 2.0f;
    genome.connections[7].weight = -1.5f;
    spec.genome = genome;

    std::string tmp_path = "test_genome_net_roundtrip.json";
    save_boid_spec(spec, tmp_path);
    BoidSpec reloaded = load_boid_spec(tmp_path);

    NeatNetwork net_original(genome);
    NeatNetwork net_reloaded(*reloaded.genome);

    float inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float out_orig[4] = {}, out_reload[4] = {};

    net_original.activate(inputs, 10, out_orig, 4);
    net_reloaded.activate(inputs, 10, out_reload, 4);

    for (int i = 0; i < 4; ++i) {
        CHECK_THAT(out_reload[i], WithinAbs(out_orig[i], 1e-5f));
    }

    std::filesystem::remove(tmp_path);
}

TEST_CASE("Spec to boid integration: fire rear thruster and move", "[boid_spec]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));
    Boid boid = create_boid_from_spec(spec);

    // Fire the rear thruster
    boid.thrusters[0].power = 1.0f;

    float dt = 0.01f;
    for (int i = 0; i < 50; i++) {
        boid.step(dt, 0, 0);
    }

    // Boid should have moved forward (+Y)
    CHECK(boid.body.position.y > 0);
    CHECK(boid.body.velocity.y > 0);
    CHECK_THAT(boid.body.position.x, WithinAbs(0.0f, 1e-3f));
}
