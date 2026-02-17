#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/world.h"
#include "io/boid_spec.h"
#include <random>

using Catch::Matchers::WithinAbs;

// Helper: create a minimal boid at a given position
static Boid make_boid_at(Vec2 pos, float energy = 100.0f) {
    Boid b;
    b.type = "prey";
    b.body.position = pos;
    b.body.mass = 1.0f;
    b.body.moment_of_inertia = 0.5f;
    b.energy = energy;
    // Give it a rear thruster so we can test thrust costs
    b.thrusters.push_back(Thruster{{0, -0.5f}, {0, 1}, 50.0f, 0.0f});
    return b;
}

// --- Food eating tests ---

TEST_CASE("Food: boid eats food within radius", "[food]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.food_eat_radius = 10.0f;
    config.food_energy = 15.0f;
    config.metabolism_rate = 0.0f;  // no metabolism for this test
    config.thrust_cost = 0.0f;
    World world(config);

    Boid b = make_boid_at({100, 100});
    world.add_boid(std::move(b));

    // Place food within eating radius
    world.add_food(Food{{105, 100}, 15.0f});

    world.step(1.0f / 120.0f);

    CHECK(world.get_food().empty());
    CHECK(world.get_boids()[0].energy > 100.0f);
    CHECK_THAT(world.get_boids()[0].total_energy_gained, WithinAbs(15.0f, 0.01f));
}

TEST_CASE("Food: boid does not eat food outside radius", "[food]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.food_eat_radius = 10.0f;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    World world(config);

    world.add_boid(make_boid_at({100, 100}));
    world.add_food(Food{{200, 200}, 10.0f});  // far away

    world.step(1.0f / 120.0f);

    CHECK(world.get_food().size() == 1);
}

TEST_CASE("Food: toroidal eating across boundary", "[food]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.toroidal = true;
    config.food_eat_radius = 10.0f;
    config.food_energy = 10.0f;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    World world(config);

    // Boid near right edge, food near left edge — toroidal distance < 10
    world.add_boid(make_boid_at({795, 400}));
    world.add_food(Food{{3, 400}, 10.0f});

    world.step(1.0f / 120.0f);

    CHECK(world.get_food().empty());
    CHECK_THAT(world.get_boids()[0].total_energy_gained, WithinAbs(10.0f, 0.01f));
}

TEST_CASE("Food: food respawns up to max", "[food]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.food_spawn_rate = 10000.0f;  // very high to guarantee spawning
    config.food_max = 5;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    World world(config);

    std::mt19937 rng(42);
    world.step(1.0f, &rng);  // large dt to trigger many spawns

    CHECK(static_cast<int>(world.get_food().size()) <= config.food_max);
    CHECK(world.get_food().size() > 0);
}

TEST_CASE("Food: no spawning without rng", "[food]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.food_spawn_rate = 10000.0f;
    World world(config);

    world.step(1.0f);  // no rng passed

    CHECK(world.get_food().empty());
}

// --- Energy tests ---

TEST_CASE("Energy: metabolism drains energy over time", "[energy]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.metabolism_rate = 10.0f;  // 10 energy/second
    config.thrust_cost = 0.0f;
    World world(config);

    world.add_boid(make_boid_at({100, 100}, 100.0f));

    float dt = 1.0f / 120.0f;
    world.step(dt);

    float expected = 100.0f - 10.0f * dt;
    CHECK_THAT(world.get_boids()[0].energy, WithinAbs(expected, 0.01f));
}

TEST_CASE("Energy: thrust costs energy", "[energy]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 1.0f;  // 1 energy per unit thrust per second
    World world(config);

    Boid b = make_boid_at({100, 100}, 100.0f);
    b.thrusters[0].power = 0.5f;  // 50% power, max_thrust=50 → 25 units thrust
    world.add_boid(std::move(b));

    float dt = 1.0f / 120.0f;
    world.step(dt);

    // Thrust cost = 1.0 * 25.0 * dt
    float expected = 100.0f - 1.0f * 25.0f * dt;
    CHECK_THAT(world.get_boids()[0].energy, WithinAbs(expected, 0.1f));
}

TEST_CASE("Energy: boid dies at zero energy", "[energy]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.metabolism_rate = 1000.0f;  // very high to kill quickly
    config.thrust_cost = 0.0f;
    World world(config);

    world.add_boid(make_boid_at({100, 100}, 1.0f));

    world.step(1.0f / 120.0f);

    CHECK(!world.get_boids()[0].alive);
    CHECK_THAT(world.get_boids()[0].energy, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Energy: dead boid does not move", "[energy]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    World world(config);

    Boid b = make_boid_at({100, 100}, 0.0f);
    b.alive = false;
    b.thrusters[0].power = 1.0f;
    Vec2 start_pos = b.body.position;
    world.add_boid(std::move(b));

    world.step(1.0f / 120.0f);

    // Dead boid should not have moved
    CHECK_THAT(world.get_boids()[0].body.position.x, WithinAbs(start_pos.x, 0.001f));
    CHECK_THAT(world.get_boids()[0].body.position.y, WithinAbs(start_pos.y, 0.001f));
}

TEST_CASE("Energy: dead boid does not eat food", "[energy]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.food_eat_radius = 10.0f;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    World world(config);

    Boid b = make_boid_at({100, 100});
    b.alive = false;
    world.add_boid(std::move(b));
    world.add_food(Food{{100, 100}, 10.0f});

    world.step(1.0f / 120.0f);

    CHECK(world.get_food().size() == 1);  // food not eaten
}

// --- Fitness metric ---

TEST_CASE("Fitness: total_energy_gained tracks cumulative food eaten", "[food][fitness]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.food_eat_radius = 10.0f;
    config.food_energy = 5.0f;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    World world(config);

    world.add_boid(make_boid_at({100, 100}));

    // Eat two food items in sequence
    world.add_food(Food{{100, 100}, 5.0f});
    world.step(1.0f / 120.0f);
    CHECK_THAT(world.get_boids()[0].total_energy_gained, WithinAbs(5.0f, 0.01f));

    world.add_food(Food{{100, 100}, 5.0f});
    world.step(1.0f / 120.0f);
    CHECK_THAT(world.get_boids()[0].total_energy_gained, WithinAbs(10.0f, 0.01f));
}

TEST_CASE("Food: boid with high thrust runs out faster", "[food][energy]") {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.5f;
    World world(config);

    Boid slow = make_boid_at({100, 100}, 50.0f);
    slow.thrusters[0].power = 0.1f;

    Boid fast = make_boid_at({300, 300}, 50.0f);
    fast.thrusters[0].power = 1.0f;

    world.add_boid(std::move(slow));
    world.add_boid(std::move(fast));

    // Step many times
    for (int i = 0; i < 600; ++i) {
        world.step(1.0f / 120.0f);
    }

    // Fast boid should have less energy (or be dead)
    if (world.get_boids()[1].alive) {
        CHECK(world.get_boids()[1].energy < world.get_boids()[0].energy);
    } else {
        CHECK(world.get_boids()[0].alive);  // slow should still be alive
    }
}
