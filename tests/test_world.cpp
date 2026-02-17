#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/world.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("World steps boid physics", "[world]") {
    WorldConfig cfg;
    cfg.toroidal = false;
    cfg.linear_drag = 0;
    cfg.angular_drag = 0;
    World world(cfg);

    Boid b;
    b.body.mass = 1.0f;
    b.thrusters.push_back({{0, -0.5f}, {0, 1}, 50.0f, 1.0f}); // rear, full power
    world.add_boid(std::move(b));

    float dt = 0.01f;
    for (int i = 0; i < 100; i++) {
        world.step(dt);
    }

    CHECK(world.get_boids()[0].body.position.y > 0);
}

TEST_CASE("Toroidal wrapping X axis", "[world]") {
    WorldConfig cfg;
    cfg.width = 100;
    cfg.height = 100;
    cfg.toroidal = true;
    cfg.linear_drag = 0;
    cfg.angular_drag = 0;
    World world(cfg);

    Boid b;
    b.body.mass = 1.0f;
    b.body.position = {95, 50};
    b.body.velocity = {20, 0}; // moving right fast
    world.add_boid(std::move(b));

    world.step(1.0f); // big step: 95 + 20 = 115 → wraps to 15

    CHECK_THAT(world.get_boids()[0].body.position.x, WithinAbs(15.0f, 1e-3f));
    CHECK_THAT(world.get_boids()[0].body.position.y, WithinAbs(50.0f, 1e-3f));
}

TEST_CASE("Toroidal wrapping Y axis", "[world]") {
    WorldConfig cfg;
    cfg.width = 100;
    cfg.height = 100;
    cfg.toroidal = true;
    cfg.linear_drag = 0;
    cfg.angular_drag = 0;
    World world(cfg);

    Boid b;
    b.body.mass = 1.0f;
    b.body.position = {50, 5};
    b.body.velocity = {0, -20}; // moving down (negative Y)
    world.add_boid(std::move(b));

    world.step(1.0f); // 5 + (-20) = -15 → wraps to 85

    CHECK_THAT(world.get_boids()[0].body.position.y, WithinAbs(85.0f, 1e-3f));
}

TEST_CASE("Multiple boids update independently", "[world]") {
    WorldConfig cfg;
    cfg.toroidal = false;
    cfg.linear_drag = 0;
    cfg.angular_drag = 0;
    World world(cfg);

    Boid a;
    a.body.mass = 1.0f;
    a.body.velocity = {10, 0};

    Boid b;
    b.body.mass = 1.0f;
    b.body.velocity = {0, -5};

    world.add_boid(std::move(a));
    world.add_boid(std::move(b));

    world.step(1.0f);

    CHECK_THAT(world.get_boids()[0].body.position.x, WithinAbs(10.0f, 1e-3f));
    CHECK_THAT(world.get_boids()[1].body.position.y, WithinAbs(-5.0f, 1e-3f));
}
