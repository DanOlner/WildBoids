#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/boid.h"

using Catch::Matchers::WithinAbs;

// Helper: create the standard 4-thruster boid layout
static Boid make_standard_boid() {
    Boid b;
    b.type = "prey";
    b.body.mass = 1.0f;
    b.body.moment_of_inertia = 0.5f;
    b.energy = 100.0f;

    // Rear (forward thrust)
    b.thrusters.push_back({{0, -0.5f}, {0, 1}, 50.0f, 0});
    // Left-rear (fires right → CCW torque)
    b.thrusters.push_back({{-0.3f, -0.4f}, {1, 0}, 20.0f, 0});
    // Right-rear (fires left → CW torque)
    b.thrusters.push_back({{0.3f, -0.4f}, {-1, 0}, 20.0f, 0});
    // Front (braking)
    b.thrusters.push_back({{0, 0.5f}, {0, -1}, 15.0f, 0});

    return b;
}

TEST_CASE("Boid with rear thruster moves forward", "[boid]") {
    Boid b = make_standard_boid();
    b.thrusters[0].power = 1.0f; // rear thruster full power

    float dt = 0.01f;
    for (int i = 0; i < 100; i++) {
        b.step(dt, 0, 0);
    }

    // Should have moved in +Y (forward)
    CHECK(b.body.position.y > 0);
    CHECK_THAT(b.body.position.x, WithinAbs(0.0f, 1e-3f));
    CHECK(b.body.velocity.y > 0);
}

TEST_CASE("Boid with left-rear thruster rotates and curves", "[boid]") {
    Boid b = make_standard_boid();
    b.thrusters[1].power = 1.0f; // left-rear only

    float dt = 0.01f;
    for (int i = 0; i < 50; i++) {
        b.step(dt, 0, 0);
    }

    // Should have rotated (left-rear fires right → CCW torque → positive angle)
    CHECK(b.body.angle > 0);
    // Should also have moved somewhat (the thruster produces some linear force too)
    float speed = b.body.velocity.length();
    CHECK(speed > 0);
}

TEST_CASE("Boid with equal side thrusters goes straight", "[boid]") {
    Boid b = make_standard_boid();
    b.thrusters[1].power = 1.0f; // left-rear
    b.thrusters[2].power = 1.0f; // right-rear

    float dt = 0.01f;
    for (int i = 0; i < 100; i++) {
        b.step(dt, 0, 0);
    }

    // Rotation should cancel out
    CHECK_THAT(b.body.angle, WithinAbs(0.0f, 1e-3f));
    // But there should be no net forward/backward motion from pure lateral thrusters
    // (they fire in opposite X directions, so X forces cancel;
    //  neither produces Y force)
    CHECK_THAT(b.body.position.y, WithinAbs(0.0f, 1e-3f));
}

TEST_CASE("Boid with no thruster power stays put", "[boid]") {
    Boid b = make_standard_boid();

    b.step(0.01f, 0, 0);

    CHECK_THAT(b.body.position.x, WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(b.body.position.y, WithinAbs(0.0f, 1e-6f));
}
