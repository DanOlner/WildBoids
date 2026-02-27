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

// ---- Speed (proprioceptive) sensor tests ----

TEST_CASE("Speed sensor: stationary boid returns 0", "[sensor][proprioception]") {
    SensorSpec spec{0, 0, 0, 0, EntityFilter::Speed, SignalType::NearestDistance};
    SensorySystem sys({spec});

    auto boids = make_boids(make_boid({400, 400}));
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.grid_cell_size = 100;
    config.max_speed = 50.0f;
    World world(config);
    world.add_boid(std::move(boids[0]));
    world.step(0);

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Speed sensor: moving boid returns normalized speed", "[sensor][proprioception]") {
    SensorSpec spec{0, 0, 0, 0, EntityFilter::Speed, SignalType::NearestDistance};
    SensorySystem sys({spec});

    Boid b = make_boid({400, 400});
    b.body.velocity = {0, 25.0f};  // moving at 25 units/s

    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.grid_cell_size = 100;
    config.max_speed = 50.0f;
    World world(config);
    world.add_boid(std::move(b));
    world.step(0);

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    // 25 / 50 = 0.5
    CHECK_THAT(output, WithinAbs(0.5f, 0.02f));
}

TEST_CASE("Speed sensor: clamps to 1.0 above max_speed", "[sensor][proprioception]") {
    SensorSpec spec{0, 0, 0, 0, EntityFilter::Speed, SignalType::NearestDistance};
    SensorySystem sys({spec});

    Boid b = make_boid({400, 400});
    b.body.velocity = {0, 80.0f};  // faster than max_speed

    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.grid_cell_size = 100;
    config.max_speed = 50.0f;
    World world(config);
    world.add_boid(std::move(b));
    world.step(0);

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    CHECK_THAT(output, WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Speed sensor: diagonal velocity uses magnitude", "[sensor][proprioception]") {
    SensorSpec spec{0, 0, 0, 0, EntityFilter::Speed, SignalType::NearestDistance};
    SensorySystem sys({spec});

    Boid b = make_boid({400, 400});
    b.body.velocity = {30.0f, 40.0f};  // magnitude = 50

    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.grid_cell_size = 100;
    config.max_speed = 50.0f;
    World world(config);
    world.add_boid(std::move(b));
    world.step(0);

    float output = 0;
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), &output);

    // sqrt(30² + 40²) / 50 = 50/50 = 1.0
    CHECK_THAT(output, WithinAbs(1.0f, 0.02f));
}

TEST_CASE("Speed sensor: works alongside external sensors", "[sensor][proprioception]") {
    // Forward boid sensor + speed sensor
    SensorSpec boid_sensor{0, 0, 36 * DEG, 100.0f, EntityFilter::Any, SignalType::NearestDistance};
    SensorSpec speed_sensor{1, 0, 0, 0, EntityFilter::Speed, SignalType::NearestDistance};
    SensorySystem sys({boid_sensor, speed_sensor});

    Boid b0 = make_boid({400, 400});
    b0.body.velocity = {0, 25.0f};

    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.grid_cell_size = 100;
    config.max_speed = 50.0f;
    World world(config);
    world.add_boid(std::move(b0));
    // Other boid 50 units ahead for the boid sensor to detect
    world.add_boid(make_boid({400, 450}));
    world.step(0);

    float outputs[2] = {0, 0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    // Boid sensor detects the other boid: 1 - (50/100) = 0.5
    CHECK_THAT(outputs[0], WithinAbs(0.5f, 0.02f));
    // Speed sensor: 25/50 = 0.5
    CHECK_THAT(outputs[1], WithinAbs(0.5f, 0.02f));
}

TEST_CASE("Speed sensor: integrated via World::step()", "[sensor][proprioception]") {
    WorldConfig config;
    config.width = 800;
    config.height = 800;
    config.toroidal = true;
    config.grid_cell_size = 100;
    config.max_speed = 50.0f;
    World world(config);

    SensorSpec speed_sensor{0, 0, 0, 0, EntityFilter::Speed, SignalType::NearestDistance};
    Boid b = make_boid({400, 400});
    b.body.velocity = {0, 30.0f};
    b.sensors.emplace(std::vector<SensorSpec>{speed_sensor});
    b.sensor_outputs.resize(1, 0.0f);
    world.add_boid(std::move(b));

    world.step(0);

    // 30/50 = 0.6 (approximately — dt=0 so velocity unchanged)
    CHECK_THAT(world.get_boids()[0].sensor_outputs[0], WithinAbs(0.6f, 0.02f));
}

// ---- Compound-eye sensor tests ----

// Helper: make a compound-eye WorldConfig with specified channels enabled
static WorldConfig make_compound_config(float world_size = 800.0f,
                                          std::vector<SensorChannel> channels = {
                                              SensorChannel::Food,
                                              SensorChannel::Same,
                                              SensorChannel::Opposite
                                          }) {
    WorldConfig config;
    config.width = world_size;
    config.height = world_size;
    config.toroidal = true;
    config.grid_cell_size = 100.0f;
    config.max_speed = 50.0f;
    config.enabled_channels = std::move(channels);
    return config;
}

// Helper: make a simple compound-eye config with N eyes, all channels
static CompoundEyeConfig make_simple_eyes(int n_eyes, float arc_deg, float max_range) {
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite};
    cfg.has_speed_sensor = true;
    float arc_rad = arc_deg * DEG;
    for (int i = 0; i < n_eyes; ++i) {
        EyeSpec eye;
        eye.id = i;
        // Spread evenly around circle
        eye.center_angle = (2.0f * PI * i) / static_cast<float>(n_eyes);
        if (eye.center_angle > PI) eye.center_angle -= 2.0f * PI; // wrap to [-π, π]
        eye.arc_width = arc_rad;
        eye.max_range = max_range;
        cfg.eyes.push_back(eye);
    }
    return cfg;
}

TEST_CASE("Compound eye: total_inputs() correct", "[sensor][compound]") {
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite};
    cfg.has_speed_sensor = true;
    for (int i = 0; i < 4; ++i) {
        cfg.eyes.push_back(EyeSpec{i, 0, 90 * DEG, 100});
    }
    // 4 eyes × 3 channels + 1 speed = 13
    CHECK(cfg.total_inputs() == 13);

    cfg.has_speed_sensor = false;
    CHECK(cfg.total_inputs() == 12);
}

TEST_CASE("Compound eye: food channel detects food ahead", "[sensor][compound]") {
    // Single forward eye, all channels
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite};
    cfg.has_speed_sensor = false;
    cfg.eyes.push_back(EyeSpec{0, 0, 90 * DEG, 100});
    SensorySystem sys(cfg);

    auto boids = make_boids(make_boid({400, 400}));
    WorldConfig wc = make_compound_config();
    World world(wc);
    world.add_boid(std::move(boids[0]));
    world.add_food(Food{{400, 450}, 10.0f}); // 50 units ahead
    world.step(0);

    // 1 eye × 3 channels = 3 outputs
    float outputs[3] = {0, 0, 0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    // Food channel (index 0): 1 - 50/100 = 0.5
    CHECK_THAT(outputs[0], WithinAbs(0.5f, 0.02f));
    // Same channel: no same-type boids
    CHECK_THAT(outputs[1], WithinAbs(0.0f, 1e-6f));
    // Opposite channel: no opposite-type boids
    CHECK_THAT(outputs[2], WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Compound eye: same channel detects same type", "[sensor][compound]") {
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite};
    cfg.has_speed_sensor = false;
    cfg.eyes.push_back(EyeSpec{0, 0, 90 * DEG, 100});
    SensorySystem sys(cfg);

    // Self is prey, other is also prey (same type) — 50 units ahead
    auto boids = make_boids(make_boid({400, 400}, 0, "prey"),
                              make_boid({400, 450}, 0, "prey"));
    WorldConfig wc = make_compound_config();
    World world(wc);
    for (auto& b : boids) world.add_boid(std::move(b));
    world.step(0);

    float outputs[3] = {0, 0, 0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    CHECK_THAT(outputs[0], WithinAbs(0.0f, 1e-6f)); // food: nothing
    CHECK_THAT(outputs[1], WithinAbs(0.5f, 0.02f));  // same: prey sees prey
    CHECK_THAT(outputs[2], WithinAbs(0.0f, 1e-6f));  // opposite: nothing
}

TEST_CASE("Compound eye: opposite channel detects different type", "[sensor][compound]") {
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite};
    cfg.has_speed_sensor = false;
    cfg.eyes.push_back(EyeSpec{0, 0, 90 * DEG, 100});
    SensorySystem sys(cfg);

    // Self is prey, other is predator (opposite type) — 50 units ahead
    auto boids = make_boids(make_boid({400, 400}, 0, "prey"),
                              make_boid({400, 450}, 0, "predator"));
    WorldConfig wc = make_compound_config();
    World world(wc);
    for (auto& b : boids) world.add_boid(std::move(b));
    world.step(0);

    float outputs[3] = {0, 0, 0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    CHECK_THAT(outputs[0], WithinAbs(0.0f, 1e-6f)); // food: nothing
    CHECK_THAT(outputs[1], WithinAbs(0.0f, 1e-6f)); // same: nothing
    CHECK_THAT(outputs[2], WithinAbs(0.5f, 0.02f));  // opposite: predator detected
}

TEST_CASE("Compound eye: multi-eye output layout", "[sensor][compound]") {
    // 2 eyes: forward (0°) and right (-90°), 3 channels + speed
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite};
    cfg.has_speed_sensor = true;
    cfg.eyes.push_back(EyeSpec{0, 0, 36 * DEG, 100});       // forward
    cfg.eyes.push_back(EyeSpec{1, -PI / 2, 36 * DEG, 100}); // right
    SensorySystem sys(cfg);

    CHECK(sys.input_count() == 7); // 2 × 3 + 1

    // Prey at (400,400), food at (400,450) directly ahead
    auto boids = make_boids(make_boid({400, 400}, 0, "prey"));
    WorldConfig wc = make_compound_config();
    World world(wc);
    world.add_boid(std::move(boids[0]));
    world.add_food(Food{{400, 450}, 10.0f});
    world.step(0);

    float outputs[7] = {};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    // Eye 0 (forward): food channel [0] should fire
    CHECK_THAT(outputs[0], WithinAbs(0.5f, 0.02f)); // eye0 food
    CHECK_THAT(outputs[1], WithinAbs(0.0f, 1e-6f)); // eye0 same
    CHECK_THAT(outputs[2], WithinAbs(0.0f, 1e-6f)); // eye0 opposite
    // Eye 1 (right): nothing to the right
    CHECK_THAT(outputs[3], WithinAbs(0.0f, 1e-6f)); // eye1 food
    CHECK_THAT(outputs[4], WithinAbs(0.0f, 1e-6f)); // eye1 same
    CHECK_THAT(outputs[5], WithinAbs(0.0f, 1e-6f)); // eye1 opposite
    // Speed sensor (stationary)
    CHECK_THAT(outputs[6], WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Compound eye: disabled channels produce zero", "[sensor][compound]") {
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food, SensorChannel::Same, SensorChannel::Opposite};
    cfg.has_speed_sensor = false;
    cfg.eyes.push_back(EyeSpec{0, 0, 90 * DEG, 100});
    SensorySystem sys(cfg);

    // Prey boid with both food and same-type boid ahead
    auto boids = make_boids(make_boid({400, 400}, 0, "prey"),
                              make_boid({400, 450}, 0, "prey"));
    // Only food channel enabled
    WorldConfig wc = make_compound_config(800.0f, {SensorChannel::Food});
    World world(wc);
    for (auto& b : boids) world.add_boid(std::move(b));
    world.add_food(Food{{400, 460}, 10.0f});
    world.step(0);

    float outputs[3] = {0, 0, 0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    // Food channel fires
    CHECK(outputs[0] > 0.0f);
    // Same and opposite disabled — should be zero despite same-type boid present
    CHECK_THAT(outputs[1], WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(outputs[2], WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Compound eye: arc filtering per eye", "[sensor][compound]") {
    // Two narrow eyes: one forward, one backward
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food};
    cfg.has_speed_sensor = false;
    cfg.eyes.push_back(EyeSpec{0, 0, 36 * DEG, 100});       // forward
    cfg.eyes.push_back(EyeSpec{1, PI, 36 * DEG, 100});       // backward
    SensorySystem sys(cfg);

    // Food directly ahead
    auto boids = make_boids(make_boid({400, 400}));
    WorldConfig wc = make_compound_config();
    World world(wc);
    world.add_boid(std::move(boids[0]));
    world.add_food(Food{{400, 450}, 10.0f});
    world.step(0);

    float outputs[2] = {0, 0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    // Forward eye detects food
    CHECK_THAT(outputs[0], WithinAbs(0.5f, 0.02f));
    // Backward eye does not
    CHECK_THAT(outputs[1], WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Compound eye: speed sensor appended at end", "[sensor][compound]") {
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Food};
    cfg.has_speed_sensor = true;
    cfg.eyes.push_back(EyeSpec{0, 0, 90 * DEG, 100});
    SensorySystem sys(cfg);

    CHECK(sys.input_count() == 2); // 1 eye × 1 channel + 1 speed

    Boid b = make_boid({400, 400});
    b.body.velocity = {0, 25.0f};

    WorldConfig wc = make_compound_config();
    World world(wc);
    world.add_boid(std::move(b));
    world.step(0);

    float outputs[2] = {0, 0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    // Eye output: no food
    CHECK_THAT(outputs[0], WithinAbs(0.0f, 1e-6f));
    // Speed: 25/50 = 0.5
    CHECK_THAT(outputs[1], WithinAbs(0.5f, 0.02f));
}

TEST_CASE("Compound eye: toroidal detection across world edge", "[sensor][compound]") {
    CompoundEyeConfig cfg;
    cfg.channels = {SensorChannel::Same};
    cfg.has_speed_sensor = false;
    cfg.eyes.push_back(EyeSpec{0, 0, 36 * DEG, 100});
    SensorySystem sys(cfg);

    // Prey near top edge, another prey near bottom edge (wraps to ~20 units ahead)
    auto boids = make_boids(make_boid({400, 790}, 0, "prey"),
                              make_boid({400, 10}, 0, "prey"));
    WorldConfig wc = make_compound_config();
    World world(wc);
    for (auto& b : boids) world.add_boid(std::move(b));
    world.step(0);

    float outputs[1] = {0};
    sys.perceive(world.get_boids(), world.grid(), world.get_config(), 0, world.get_food(), outputs);

    // Distance: 800 - 790 + 10 = 20, signal: 1 - 20/100 = 0.8
    CHECK_THAT(outputs[0], WithinAbs(0.8f, 0.02f));
}
