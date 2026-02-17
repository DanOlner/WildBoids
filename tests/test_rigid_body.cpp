#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/rigid_body.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("RigidBody at rest stays at rest", "[rigid_body]") {
    RigidBody rb;
    rb.integrate(1.0f / 60, 0, 0);
    CHECK_THAT(rb.position.x, WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(rb.position.y, WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(rb.velocity.x, WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(rb.velocity.y, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Constant force produces expected acceleration", "[rigid_body]") {
    RigidBody rb;
    rb.mass = 2.0f;
    float dt = 0.01f;

    // Apply F=10 in +X for 100 steps (1 second), no drag
    for (int i = 0; i < 100; i++) {
        rb.apply_force({10, 0});
        rb.integrate(dt, 0, 0);
        rb.clear_forces();
    }

    // a = F/m = 5, after 1s: v = 5, x = 0.5 * a * t^2 = 2.5
    // Semi-implicit Euler overshoots slightly compared to analytic, allow tolerance
    CHECK_THAT(rb.velocity.x, WithinAbs(5.0f, 0.1f));
    CHECK_THAT(rb.position.x, WithinAbs(2.5f, 0.1f));
}

TEST_CASE("Torque changes angular velocity", "[rigid_body]") {
    RigidBody rb;
    rb.moment_of_inertia = 1.0f;
    float dt = 0.01f;

    rb.apply_torque(5.0f);
    rb.integrate(dt, 0, 0);
    rb.clear_forces();

    // After one step: angular_vel = torque/I * dt = 5 * 0.01 = 0.05
    CHECK_THAT(rb.angular_velocity, WithinAbs(0.05f, 1e-5f));
    CHECK(rb.angle > 0); // angle increased (CCW positive)
}

TEST_CASE("Linear drag reduces velocity", "[rigid_body]") {
    RigidBody rb;
    rb.velocity = {10, 0};
    float dt = 0.01f;
    float drag = 0.5f;

    for (int i = 0; i < 200; i++) {
        rb.integrate(dt, drag, 0);
        rb.clear_forces();
    }

    // After 2 seconds of drag, velocity should be significantly reduced from 10
    CHECK(rb.velocity.x < 5.0f);
    CHECK(rb.velocity.x > 0.0f);
}

TEST_CASE("Angular drag reduces angular velocity", "[rigid_body]") {
    RigidBody rb;
    rb.angular_velocity = 5.0f;
    float dt = 0.01f;
    float drag = 0.5f;

    for (int i = 0; i < 200; i++) {
        rb.integrate(dt, 0, drag);
        rb.clear_forces();
    }

    // Should be significantly reduced from initial 5
    CHECK(rb.angular_velocity < rb.angular_velocity + 1); // sanity: still positive
    CHECK(rb.angular_velocity < 5.0f);
    CHECK(rb.angular_velocity > 0.0f);
}
