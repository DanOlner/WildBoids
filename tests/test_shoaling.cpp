#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/world.h"

using Catch::Matchers::WithinAbs;

static Boid make_boid_at(const std::string& type, Vec2 pos) {
    Boid b;
    b.type = type;
    b.body.position = pos;
    b.body.mass = 1.0f;
    b.body.moment_of_inertia = 0.5f;
    b.energy = 100.0f;
    b.thrusters.push_back(Thruster{{0, -0.5f}, {0, 1}, 50.0f, 0.0f});
    return b;
}

static constexpr float PI = 3.14159265f;

static WorldConfig shoaling_config() {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.toroidal = false;
    config.linear_drag = 0.2f;
    config.angular_drag = 0.1f;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    config.prey_shoaling.radius = 50.0f;
    config.prey_shoaling.max_reduction = 0.3f;
    config.prey_shoaling.max_neighbours = 6;
    config.prey_shoaling.arc = 2.0f * PI;  // full circle for existing tests
    config.predator_shoaling.radius = 50.0f;
    config.predator_shoaling.max_reduction = 0.2f;
    config.predator_shoaling.max_neighbours = 4;
    config.predator_shoaling.arc = 2.0f * PI;
    return config;
}

TEST_CASE("Shoaling: disabled when radius is zero", "[shoaling]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.linear_drag = 0.2f;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    // prey_shoaling and predator_shoaling default to radius 0 = disabled
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("prey", {110, 100}));

    world.step(1.0f / 60.0f);

    // With shoaling disabled, effective_linear_drag should equal world drag
    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(config.linear_drag, 1e-6f));
    CHECK_THAT(world.get_boids()[1].effective_linear_drag,
               WithinAbs(config.linear_drag, 1e-6f));
}

TEST_CASE("Shoaling: lone boid gets no drag reduction", "[shoaling]") {
    auto config = shoaling_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {400, 400}));

    world.step(1.0f / 60.0f);

    // No neighbours → effective drag = world drag
    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(config.linear_drag, 1e-6f));
}

TEST_CASE("Shoaling: prey near same-type neighbours gets reduced drag", "[shoaling]") {
    auto config = shoaling_config();
    World world(config);

    // Central prey with 3 neighbours within radius 50
    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("prey", {120, 100}));  // dist 20
    world.add_boid(make_boid_at("prey", {100, 130}));  // dist 30
    world.add_boid(make_boid_at("prey", {110, 110}));  // dist ~14

    world.step(1.0f / 60.0f);

    // Boid 0 has 3 neighbours. fraction = 3/6 = 0.5
    // effective_drag = 0.2 * (1 - 0.3 * 0.5) = 0.2 * 0.85 = 0.17
    float expected = 0.2f * (1.0f - 0.3f * (3.0f / 6.0f));
    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(expected, 1e-4f));
}

TEST_CASE("Shoaling: opposite-type neighbours don't count", "[shoaling]") {
    auto config = shoaling_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("predator", {110, 100}));  // nearby but different type

    world.step(1.0f / 60.0f);

    // Prey has no same-type neighbours → no reduction
    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(config.linear_drag, 1e-6f));
}

TEST_CASE("Shoaling: neighbours beyond radius don't count", "[shoaling]") {
    auto config = shoaling_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("prey", {200, 100}));  // dist 100 > radius 50

    world.step(1.0f / 60.0f);

    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(config.linear_drag, 1e-6f));
}

TEST_CASE("Shoaling: reduction caps at max neighbours", "[shoaling]") {
    auto config = shoaling_config();
    World world(config);

    // Central prey with 10 neighbours (more than max_neighbours=6)
    world.add_boid(make_boid_at("prey", {100, 100}));
    for (int i = 0; i < 10; ++i) {
        world.add_boid(make_boid_at("prey", {100.0f + (i + 1) * 3.0f, 100}));
    }

    world.step(1.0f / 60.0f);

    // Max reduction: 0.2 * (1 - 0.3) = 0.14
    float max_reduced = config.linear_drag * (1.0f - config.prey_shoaling.max_reduction);
    CHECK(world.get_boids()[0].effective_linear_drag >= max_reduced - 1e-4f);
}

TEST_CASE("Shoaling: predator gets its own reduction rate", "[shoaling]") {
    auto config = shoaling_config();
    World world(config);

    // 1 predator with 4 predator neighbours (at max)
    world.add_boid(make_boid_at("predator", {100, 100}));
    for (int i = 0; i < 4; ++i) {
        world.add_boid(make_boid_at("predator", {100.0f + (i + 1) * 5.0f, 100}));
    }

    world.step(1.0f / 60.0f);

    // fraction = 4/4 = 1.0, max_reduction = 0.2
    // effective_drag = 0.2 * (1 - 0.2) = 0.16
    float expected = config.linear_drag * (1.0f - config.predator_shoaling.max_reduction);
    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(expected, 1e-4f));
}

TEST_CASE("Shoaling: boid with reduced drag coasts further", "[shoaling]") {
    // Two identical boids with identical initial velocity,
    // one with neighbours (reduced drag), one alone.
    auto config = shoaling_config();
    config.prey_shoaling.max_reduction = 0.5f;  // big reduction for clear effect
    config.prey_shoaling.max_neighbours = 1;

    // World with neighbours
    World world_group(config);
    auto boid_a = make_boid_at("prey", {100, 100});
    boid_a.body.velocity = {0, 10};  // moving forward
    world_group.add_boid(std::move(boid_a));
    world_group.add_boid(make_boid_at("prey", {110, 100}));  // neighbour

    // World alone
    World world_solo(config);
    auto boid_b = make_boid_at("prey", {100, 100});
    boid_b.body.velocity = {0, 10};
    world_solo.add_boid(std::move(boid_b));

    // Run both for several ticks to let shoaling kick in (one-tick delay)
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        world_group.step(dt);
        world_solo.step(dt);
    }

    // Boid in group should have traveled further (less drag)
    float y_group = world_group.get_boids()[0].body.position.y;
    float y_solo = world_solo.get_boids()[0].body.position.y;
    CHECK(y_group > y_solo);
}

TEST_CASE("Shoaling: neighbours behind boid excluded by arc", "[shoaling]") {
    auto config = shoaling_config();
    config.prey_shoaling.arc = PI;  // front 180° only
    config.prey_shoaling.max_neighbours = 2;
    World world(config);

    // Boid at (100,100) facing angle 0 → forward is +Y
    auto central = make_boid_at("prey", {100, 100});
    central.body.angle = 0.0f;
    world.add_boid(std::move(central));

    // Neighbour ahead (+Y): should count
    world.add_boid(make_boid_at("prey", {100, 130}));
    // Neighbour behind (-Y): should NOT count
    world.add_boid(make_boid_at("prey", {100, 70}));

    world.step(1.0f / 60.0f);

    // Only 1 neighbour counts (the one ahead), fraction = 1/2
    float expected = 0.2f * (1.0f - 0.3f * (1.0f / 2.0f));
    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(expected, 1e-4f));
}

TEST_CASE("Shoaling: 270 arc includes sides but excludes directly behind", "[shoaling]") {
    auto config = shoaling_config();
    config.prey_shoaling.arc = 1.5f * PI;  // 270°
    config.prey_shoaling.max_neighbours = 3;
    World world(config);

    // Boid facing +Y (angle 0)
    auto central = make_boid_at("prey", {100, 100});
    central.body.angle = 0.0f;
    world.add_boid(std::move(central));

    // Ahead (+Y): counts
    world.add_boid(make_boid_at("prey", {100, 130}));
    // Right (+X): counts (within 270°)
    world.add_boid(make_boid_at("prey", {130, 100}));
    // Directly behind (-Y): does NOT count (outside 270°)
    world.add_boid(make_boid_at("prey", {100, 70}));

    world.step(1.0f / 60.0f);

    // 2 of 3 neighbours count, fraction = 2/3
    float expected = 0.2f * (1.0f - 0.3f * (2.0f / 3.0f));
    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(expected, 1e-4f));
}

TEST_CASE("Shoaling: dead boids don't count as neighbours", "[shoaling]") {
    auto config = shoaling_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    auto dead_boid = make_boid_at("prey", {110, 100});
    dead_boid.alive = false;
    world.add_boid(std::move(dead_boid));

    world.step(1.0f / 60.0f);

    CHECK_THAT(world.get_boids()[0].effective_linear_drag,
               WithinAbs(config.linear_drag, 1e-6f));
}
