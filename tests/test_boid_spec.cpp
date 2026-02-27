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
    REQUIRE(spec.thrusters.size() == 6);

    CHECK(spec.thrusters[0].label == "rear");
    CHECK(spec.thrusters[0].max_thrust > 0);
    CHECK(spec.thrusters[3].label == "front");
    CHECK(spec.thrusters[4].label == "strafe_left");
    CHECK(spec.thrusters[5].label == "strafe_right");

    // Compound eyes
    REQUIRE(spec.compound_eyes.has_value());
    CHECK(spec.sensors.empty());
    CHECK(spec.compound_eyes->eyes.size() == 16);
    CHECK(spec.compound_eyes->channels.size() == 3);
    CHECK(spec.compound_eyes->has_speed_sensor == true);
    CHECK(spec.compound_eyes->eyes[0].id == 0);
    CHECK_THAT(spec.compound_eyes->eyes[0].center_angle, WithinAbs(0.0f, 1e-4f));
    // Input count derived from spec — resilient to toggling proprioceptive sensors
    CHECK(sensor_input_count(spec) == spec.compound_eyes->total_inputs());
}

TEST_CASE("Create boid from spec", "[boid_spec]") {
    BoidSpec spec = load_boid_spec(data_path("simple_boid.json"));
    Boid boid = create_boid_from_spec(spec);

    CHECK(boid.type == "prey");
    CHECK_THAT(boid.body.mass, WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(boid.body.moment_of_inertia, WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(boid.energy, WithinAbs(100.0f, 1e-6f));
    REQUIRE(boid.thrusters.size() == 6);

    // All thrusters start at zero power
    for (const auto& t : boid.thrusters) {
        CHECK_THAT(t.power, WithinAbs(0.0f, 1e-6f));
    }

    // Compound-eye sensors wired up — count matches spec
    REQUIRE(boid.sensors.has_value());
    CHECK(boid.sensors->is_compound());
    int expected_inputs = sensor_input_count(spec);
    CHECK(boid.sensors->input_count() == expected_inputs);
    CHECK(boid.sensor_outputs.size() == static_cast<size_t>(expected_inputs));
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

    // Compound eyes round-trip
    REQUIRE(reloaded.compound_eyes.has_value());
    REQUIRE(original.compound_eyes.has_value());
    const auto& re = reloaded.compound_eyes->eyes;
    const auto& oe = original.compound_eyes->eyes;
    REQUIRE(re.size() == oe.size());
    for (size_t i = 0; i < oe.size(); i++) {
        CHECK(re[i].id == oe[i].id);
        CHECK_THAT(re[i].center_angle, WithinAbs(oe[i].center_angle, 1e-3f));
        CHECK_THAT(re[i].arc_width, WithinAbs(oe[i].arc_width, 1e-3f));
        CHECK_THAT(re[i].max_range, WithinAbs(oe[i].max_range, 1e-4f));
    }
    CHECK(reloaded.compound_eyes->channels.size() == original.compound_eyes->channels.size());
    CHECK(reloaded.compound_eyes->has_speed_sensor == original.compound_eyes->has_speed_sensor);

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

    // Attach a minimal genome (10 sensors → 6 thrusters)
    int next_innov = 1;
    NeatGenome genome = NeatGenome::minimal(10, 6, next_innov);
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
    NeatGenome genome = NeatGenome::minimal(10, 6, next_innov);
    genome.connections[0].weight = 2.0f;
    genome.connections[7].weight = -1.5f;
    spec.genome = genome;

    std::string tmp_path = "test_genome_net_roundtrip.json";
    save_boid_spec(spec, tmp_path);
    BoidSpec reloaded = load_boid_spec(tmp_path);

    NeatNetwork net_original(genome);
    NeatNetwork net_reloaded(*reloaded.genome);

    float inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float out_orig[6] = {}, out_reload[6] = {};

    net_original.activate(inputs, 10, out_orig, 6);
    net_reloaded.activate(inputs, 10, out_reload, 6);

    for (int i = 0; i < 6; ++i) {
        CHECK_THAT(out_reload[i], WithinAbs(out_orig[i], 1e-5f));
    }

    std::filesystem::remove(tmp_path);
}

TEST_CASE("Compound-eye spec round-trip: save and reload", "[boid_spec][compound]") {
    static constexpr float DEG = static_cast<float>(M_PI) / 180.0f;

    BoidSpec spec;
    spec.version = "0.2";
    spec.type = "prey";
    spec.mass = 1.0f;
    spec.moment_of_inertia = 0.5f;
    spec.initial_energy = 100.0f;

    // Add thrusters
    ThrusterSpec t;
    t.id = 0; t.label = "rear"; t.position = {0, -0.5f};
    t.direction = {0, 1}; t.max_thrust = 5.0f;
    spec.thrusters.push_back(t);

    // Build compound eyes: 3 eyes, 3 channels, speed sensor
    CompoundEyeConfig cfg;
    cfg.eyes.push_back(EyeSpec{0, 0, 36 * DEG, 100});
    cfg.eyes.push_back(EyeSpec{1, -60 * DEG, 45 * DEG, 80});
    cfg.eyes.push_back(EyeSpec{2, 60 * DEG, 45 * DEG, 80});
    cfg.channels = {SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite};
    cfg.has_speed_sensor = true;
    spec.compound_eyes = cfg;

    std::string tmp_path = "test_compound_roundtrip.json";
    save_boid_spec(spec, tmp_path);

    BoidSpec reloaded = load_boid_spec(tmp_path);

    // Should have compound eyes, not legacy sensors
    REQUIRE(reloaded.compound_eyes.has_value());
    CHECK(reloaded.sensors.empty());

    const auto& rc = *reloaded.compound_eyes;
    REQUIRE(rc.eyes.size() == 3);
    REQUIRE(rc.channels.size() == 3);
    CHECK(rc.has_speed_sensor == true);

    // Check eye values round-trip (degrees ↔ radians conversion)
    for (size_t i = 0; i < cfg.eyes.size(); ++i) {
        CHECK(rc.eyes[i].id == cfg.eyes[i].id);
        CHECK_THAT(rc.eyes[i].center_angle, WithinAbs(cfg.eyes[i].center_angle, 1e-3f));
        CHECK_THAT(rc.eyes[i].arc_width, WithinAbs(cfg.eyes[i].arc_width, 1e-3f));
        CHECK_THAT(rc.eyes[i].max_range, WithinAbs(cfg.eyes[i].max_range, 1e-4f));
    }

    // Check channels round-trip
    CHECK(rc.channels[0] == SensorChannel::Food);
    CHECK(rc.channels[1] == SensorChannel::Same);
    CHECK(rc.channels[2] == SensorChannel::Opposite);

    // Check total inputs
    CHECK(sensor_input_count(reloaded) == 10); // 3 × 3 + 1

    // Create boid from compound spec — should have compound SensorySystem
    Boid boid = create_boid_from_spec(reloaded);
    REQUIRE(boid.sensors.has_value());
    CHECK(boid.sensors->is_compound());
    CHECK(boid.sensors->input_count() == 10);
    CHECK(boid.sensor_outputs.size() == 10);

    std::filesystem::remove(tmp_path);
}

TEST_CASE("sensor_input_count: legacy vs compound", "[boid_spec][compound]") {
    BoidSpec legacy;
    legacy.sensors.push_back(SensorSpec{0, 0, 0, 100, EntityFilter::Any, SignalType::NearestDistance});
    legacy.sensors.push_back(SensorSpec{1, 0, 0, 100, EntityFilter::Food, SignalType::NearestDistance});
    CHECK(sensor_input_count(legacy) == 2);

    BoidSpec compound;
    CompoundEyeConfig cfg;
    cfg.eyes.push_back(EyeSpec{0, 0, 1.0f, 100});
    cfg.channels = {SensorChannel::Food, SensorChannel::Same};
    cfg.has_speed_sensor = true;
    compound.compound_eyes = cfg;
    CHECK(sensor_input_count(compound) == 3); // 1 × 2 + 1
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
