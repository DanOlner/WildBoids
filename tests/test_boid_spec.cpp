#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "io/boid_spec.h"
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

    CHECK(spec.version == "0.1");
    CHECK(spec.type == "prey");
    CHECK_THAT(spec.mass, WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(spec.moment_of_inertia, WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(spec.initial_energy, WithinAbs(100.0f, 1e-6f));
    REQUIRE(spec.thrusters.size() == 4);

    CHECK(spec.thrusters[0].label == "rear");
    CHECK_THAT(spec.thrusters[0].max_thrust, WithinAbs(50.0f, 1e-6f));
    CHECK(spec.thrusters[3].label == "front");
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

    // Clean up
    std::filesystem::remove(tmp_path);
}

TEST_CASE("Invalid file throws", "[boid_spec]") {
    CHECK_THROWS(load_boid_spec("nonexistent_file.json"));
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
