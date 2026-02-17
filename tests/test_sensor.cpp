#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/sensor.h"
#include "simulation/sensory_system.h"
#include "simulation/boid.h"
#include "simulation/world.h"
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

static constexpr float PI = static_cast<float>(M_PI);
static constexpr float DEG = PI / 180.0f;

// Helper: create a minimal boid at a given position and angle
static Boid make_boid(Vec2 pos, float angle = 0, const std::string& type = "prey") {
    Boid b;
    b.type = type;
    b.body.position = pos;
    b.body.angle = angle;
    b.body.mass = 1;
    b.body.moment_of_inertia = 1;
    return b;
}

// Helper: build a vector of boids from variadic args (avoids initializer_list copy)
template <typename... Args>
static std::vector<Boid> make_boids(Args&&... args) {
    std::vector<Boid> v;
    (v.push_back(std::forward<Args>(args)), ...);
    return v;
}

// Helper: create a world with boids and run one step to build the grid
static World make_world(std::vector<Boid> boids, float world_size = 800.0f) {
    WorldConfig config;
    config.width = world_size;
    config.height = world_size;
    config.toroidal = true;
    config.grid_cell_size = 100.0f;
    World world(config);
    for (auto& b : boids) {
        world.add_boid(std::move(b));
    }
    // Step with dt=0 just to build the grid (no physics change)
    world.step(0);
    return world;
}

// ---- angle_in_arc tests ----

TEST_CASE("angle_in_arc: center hit", "[sensor]") {
    CHECK(angle_in_arc(0, 0, 36 * DEG));
    CHECK(angle_in_arc(0.5f, 0.5f, 36 * DEG));
}

TEST_CASE("angle_in_arc: edge of arc", "[sensor]") {
    // Exactly at half arc width
    CHECK(angle_in_arc(18 * DEG, 0, 36 * DEG));
    CHECK(angle_in_arc(-18 * DEG, 0, 36 * DEG));
}

TEST_CASE("angle_in_arc: outside arc", "[sensor]") {
    CHECK_FALSE(angle_in_arc(30 * DEG, 0, 36 * DEG));
    CHECK_FALSE(angle_in_arc(-30 * DEG, 0, 36 * DEG));
}

TEST_CASE("angle_in_arc: wrapping around ±π", "[sensor]") {
    // Sensor pointing backward (center = π), arc = 90°
    // Angle at 170° should be inside
    CHECK(angle_in_arc(170 * DEG, 180 * DEG, 90 * DEG));
    // Angle at -170° (= 190°) should be inside
    CHECK(angle_in_arc(-170 * DEG, 180 * DEG, 90 * DEG));
    // Angle at 0° should be outside
    CHECK_FALSE(angle_in_arc(0, 180 * DEG, 90 * DEG));
}

// ---- SensorySystem perception tests ----

TEST_CASE("Single boid directly ahead detected", "[sensor]") {
    // Sensor: forward-facing, 36° arc, 100 range
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Self at (400,400) facing up (+Y), other at (400,450) — 50 units ahead
    auto boids = make_boids(make_boid({400, 400}), make_boid({400, 450}));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    // Expected: 1 - (50/100) = 0.5
    CHECK_THAT(output, WithinAbs(0.5f, 0.02f));
}

TEST_CASE("Boid outside arc not detected", "[sensor]") {
    // Sensor: forward-facing, narrow 36° arc
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Other boid directly to the right (90° from forward) — outside 18° half-arc
    auto boids = make_boids(make_boid({400, 400}), make_boid({450, 400}));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Boid outside range not detected", "[sensor]") {
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Other boid 150 units ahead — beyond 100 range
    auto boids = make_boids(make_boid({400, 400}), make_boid({400, 550}));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Self not detected", "[sensor]") {
    // Wide sensor that would catch everything
    SensorSpec spec{0, 0, 360 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Only one boid in the world
    auto boids = make_boids(make_boid({400, 400}));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Toroidal wrap: boid across world edge detected", "[sensor]") {
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Self near top edge facing up, other near bottom edge (wraps to ~30 units ahead)
    auto boids = make_boids(make_boid({400, 790}), make_boid({400, 10}));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    // Distance across wrap: 800 - 790 + 10 = 20 units
    // Expected: 1 - (20/100) = 0.8
    CHECK_THAT(output, WithinAbs(0.8f, 0.02f));
}

TEST_CASE("EntityFilter: prey sensor ignores predators", "[sensor]") {
    SensorSpec spec{0, 0, 360 * DEG, 100.0f, EntityFilter::Prey, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Self is prey, other is predator directly ahead
    auto boids = make_boids(make_boid({400, 400}, 0, "prey"),
                              make_boid({400, 450}, 0, "predator"));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f)); // predator filtered out
}

TEST_CASE("Rotated boid: sensor arc moves with heading", "[sensor]") {
    // Sensor: forward arc (center=0)
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Self facing right (angle = -π/2, since +Y is forward, -π/2 rotates forward to +X)
    // Other boid is 50 units to the right — which is now "forward" for the rotated boid
    float heading = -PI / 2.0f; // facing +X
    auto boids = make_boids(make_boid({400, 400}, heading), make_boid({450, 400}));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    // Should detect: 1 - (50/100) = 0.5
    CHECK_THAT(output, WithinAbs(0.5f, 0.02f));
}

TEST_CASE("SectorDensity signal type", "[sensor]") {
    SensorSpec spec{0, 0, 360 * DEG, 100.0f, EntityFilter::Any, SignalType::SectorDensity};
    SensorySystem sys({spec});

    // Self + 3 others nearby
    auto boids = make_boids(make_boid({400, 400}),
                              make_boid({410, 400}),
                              make_boid({420, 400}),
                              make_boid({430, 400}));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    // 3 neighbours / expected_max(10) = 0.3
    CHECK_THAT(output, WithinAbs(0.3f, 0.02f));
}

TEST_CASE("Multiple sensors produce independent outputs", "[sensor]") {
    // Forward sensor and right sensor
    SensorSpec forward{0, 0, 36 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorSpec right{1, -PI / 2.0f, 36 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorySystem sys({forward, right});

    // Other boid directly ahead
    auto boids = make_boids(make_boid({400, 400}), make_boid({400, 450}));
    World world = make_world(std::move(boids));

    float outputs[2] = {0, 0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    CHECK(outputs[0] > 0.0f);  // forward sensor detects
    CHECK_THAT(outputs[1], WithinAbs(0.0f, 1e-6f));  // right sensor does not
}

TEST_CASE("World runs sensors each tick", "[sensor]") {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.grid_cell_size = 100;
    World world(config);

    // Create boid with sensors
    SensorSpec spec{0, 0, 360 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    Boid b0 = make_boid({400, 400});
    b0.sensors.emplace(std::vector<SensorSpec>{spec});
    b0.sensor_outputs.resize(1, 0.0f);
    world.add_boid(std::move(b0));

    // Add a nearby boid
    world.add_boid(make_boid({410, 400}));

    world.step(0);

    // Sensor should have detected the nearby boid
    CHECK(world.get_boids()[0].sensor_outputs[0] > 0.0f);
}

// ---- Food sensor tests ----

TEST_CASE("Food sensor detects food within arc and range", "[sensor][food]") {
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Food, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Boid at (400,400) facing up, food at (400,450) — 50 units ahead
    auto boids = make_boids(make_boid({400, 400}));
    World world = make_world(std::move(boids));
    world.add_food(Food{{400, 450}, 10.0f});

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    // Expected: 1 - (50/100) = 0.5
    CHECK_THAT(output, WithinAbs(0.5f, 0.02f));
}

TEST_CASE("Food sensor ignores boids", "[sensor][food]") {
    SensorSpec spec{0, 0, 360 * DEG, 100.0f, EntityFilter::Food, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Only boids nearby, no food
    auto boids = make_boids(make_boid({400, 400}), make_boid({410, 400}));
    World world = make_world(std::move(boids));

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Boid sensor ignores food", "[sensor][food]") {
    SensorSpec spec{0, 0, 360 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Only food nearby, no other boids
    auto boids = make_boids(make_boid({400, 400}));
    World world = make_world(std::move(boids));
    world.add_food(Food{{410, 400}, 10.0f});

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Food sensor: food outside arc not detected", "[sensor][food]") {
    // Narrow forward arc
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Food, SignalType::NearestDistance};
    SensorySystem sys({spec});

    auto boids = make_boids(make_boid({400, 400}));
    World world = make_world(std::move(boids));
    // Food directly to the right — outside 18° half-arc
    world.add_food(Food{{450, 400}, 10.0f});

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Food sensor: food outside range not detected", "[sensor][food]") {
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Food, SignalType::NearestDistance};
    SensorySystem sys({spec});

    auto boids = make_boids(make_boid({400, 400}));
    World world = make_world(std::move(boids));
    // Food 150 units ahead — beyond 100 range
    world.add_food(Food{{400, 550}, 10.0f});

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Food sensor: toroidal food detection across world edge", "[sensor][food]") {
    SensorSpec spec{0, 0, 36 * DEG, 100.0f, EntityFilter::Food, SignalType::NearestDistance};
    SensorySystem sys({spec});

    // Boid near top edge facing up, food near bottom edge (wraps to ~20 units ahead)
    auto boids = make_boids(make_boid({400, 790}));
    World world = make_world(std::move(boids));
    world.add_food(Food{{400, 10}, 10.0f});

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    // Distance across wrap: 800 - 790 + 10 = 20 units
    // Expected: 1 - (20/100) = 0.8
    CHECK_THAT(output, WithinAbs(0.8f, 0.02f));
}

TEST_CASE("World runs food sensors via step()", "[sensor][food]") {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.grid_cell_size = 100;
    World world(config);

    // Create boid with a food sensor
    SensorSpec food_sensor{0, 0, 360 * DEG, 100.0f, EntityFilter::Food, SignalType::NearestDistance};
    Boid b0 = make_boid({400, 400});
    b0.sensors.emplace(std::vector<SensorSpec>{food_sensor});
    b0.sensor_outputs.resize(1, 0.0f);
    world.add_boid(std::move(b0));

    // Add food nearby
    world.add_food(Food{{420, 400}, 10.0f});

    world.step(0);

    // Food sensor should have detected the food
    CHECK(world.get_boids()[0].sensor_outputs[0] > 0.0f);
}
