#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/world.h"
#include <random>

using Catch::Matchers::WithinAbs;

// Helper: create a minimal boid at a given position
static Boid make_boid_at(const std::string& type, Vec2 pos, float energy = 100.0f) {
    Boid b;
    b.type = type;
    b.body.position = pos;
    b.body.mass = 1.0f;
    b.body.moment_of_inertia = 0.5f;
    b.energy = energy;
    b.thrusters.push_back(Thruster{{0, -0.5f}, {0, 1}, 50.0f, 0.0f});
    return b;
}

static WorldConfig predation_config() {
    WorldConfig config;
    config.width = 800; config.height = 800;
    config.metabolism_rate = 0.0f;
    config.thrust_cost = 0.0f;
    config.predator_catch_radius = 15.0f;
    config.predator_catch_energy = 50.0f;
    return config;
}

TEST_CASE("Predation: predator catches prey within catch_radius", "[predation]") {
    auto config = predation_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("predator", {110, 100}));  // distance = 10 < 15

    world.step(1.0f / 120.0f);

    CHECK(!world.get_boids()[0].alive);  // prey dead
    CHECK(world.get_boids()[1].alive);   // predator alive
    CHECK_THAT(world.get_boids()[1].total_energy_gained, WithinAbs(50.0f, 0.01f));
}

TEST_CASE("Predation: predator does not catch prey outside catch_radius", "[predation]") {
    auto config = predation_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("predator", {200, 100}));  // distance = 100 > 15

    world.step(1.0f / 120.0f);

    CHECK(world.get_boids()[0].alive);
    CHECK_THAT(world.get_boids()[1].total_energy_gained, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("Predation: dead prey is not caught again", "[predation]") {
    auto config = predation_config();
    World world(config);

    Boid prey = make_boid_at("prey", {100, 100});
    prey.alive = false;
    prey.energy = 0.0f;
    world.add_boid(std::move(prey));
    world.add_boid(make_boid_at("predator", {105, 100}));

    world.step(1.0f / 120.0f);

    CHECK_THAT(world.get_boids()[1].total_energy_gained, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("Predation: dead predator does not catch", "[predation]") {
    auto config = predation_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    Boid pred = make_boid_at("predator", {105, 100});
    pred.alive = false;
    pred.energy = 0.0f;
    world.add_boid(std::move(pred));

    world.step(1.0f / 120.0f);

    CHECK(world.get_boids()[0].alive);  // prey should survive
}

TEST_CASE("Predation: toroidal catch across world edge", "[predation]") {
    auto config = predation_config();
    config.toroidal = true;
    World world(config);

    // Prey near right edge, predator near left edge — toroidal distance < 15
    world.add_boid(make_boid_at("prey", {795, 400}));
    world.add_boid(make_boid_at("predator", {5, 400}));  // toroidal distance = 10

    world.step(1.0f / 120.0f);

    CHECK(!world.get_boids()[0].alive);  // prey caught across boundary
    CHECK_THAT(world.get_boids()[1].total_energy_gained, WithinAbs(50.0f, 0.01f));
}

TEST_CASE("Predation: predators do not eat food", "[predation]") {
    auto config = predation_config();
    config.food_eat_radius = 10.0f;
    config.food_energy = 15.0f;
    World world(config);

    world.add_boid(make_boid_at("predator", {100, 100}));
    world.add_food(Food{{100, 100}, 15.0f});

    world.step(1.0f / 120.0f);

    CHECK(world.get_food().size() == 1);  // food not eaten
    CHECK_THAT(world.get_boids()[0].total_energy_gained, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("Predation: prey do not catch other prey", "[predation]") {
    auto config = predation_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("prey", {105, 100}));  // within catch_radius but both prey

    world.step(1.0f / 120.0f);

    CHECK(world.get_boids()[0].alive);
    CHECK(world.get_boids()[1].alive);
}

TEST_CASE("Predation: predator catches multiple prey in one tick", "[predation]") {
    auto config = predation_config();
    World world(config);

    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("prey", {105, 100}));
    world.add_boid(make_boid_at("predator", {103, 100}));  // within radius of both prey

    world.step(1.0f / 120.0f);

    CHECK(!world.get_boids()[0].alive);
    CHECK(!world.get_boids()[1].alive);
    CHECK_THAT(world.get_boids()[2].total_energy_gained, WithinAbs(100.0f, 0.01f));
}

TEST_CASE("Predation: predator energy and fitness track correctly over time", "[predation]") {
    auto config = predation_config();
    World world(config);

    // Place two prey spaced apart — predator catches one per tick
    world.add_boid(make_boid_at("prey", {100, 100}));
    world.add_boid(make_boid_at("prey", {500, 500}));  // far away
    world.add_boid(make_boid_at("predator", {105, 100}));  // near first prey

    world.step(1.0f / 120.0f);

    // First prey caught, second still alive
    CHECK(!world.get_boids()[0].alive);
    CHECK(world.get_boids()[1].alive);
    CHECK_THAT(world.get_boids()[2].total_energy_gained, WithinAbs(50.0f, 0.01f));
}
