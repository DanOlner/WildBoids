#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/thruster.h"
#include <numbers>

using Catch::Matchers::WithinAbs;

TEST_CASE("Rear thruster produces forward force", "[thruster]") {
    // Rear thruster: behind center, fires in +Y (forward) direction
    Thruster t;
    t.local_position = {0, -0.5f};
    t.local_direction = {0, 1};
    t.max_thrust = 50.0f;
    t.power = 1.0f;

    // Body facing +Y (angle = 0)
    Vec2 force = t.world_force(0);
    CHECK_THAT(force.x, WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(force.y, WithinAbs(50.0f, 1e-5f));
}

TEST_CASE("Thruster at zero power produces no force", "[thruster]") {
    Thruster t;
    t.local_position = {0, -0.5f};
    t.local_direction = {0, 1};
    t.max_thrust = 50.0f;
    t.power = 0;

    Vec2 force = t.world_force(0);
    CHECK_THAT(force.x, WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(force.y, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Left-rear thruster produces torque", "[thruster]") {
    // Left-rear thruster: fires in +X (rightward in body frame)
    // Position is left of center and behind
    Thruster t;
    t.local_position = {-0.3f, -0.4f};
    t.local_direction = {1, 0};
    t.max_thrust = 20.0f;
    t.power = 1.0f;

    float torq = t.torque(0);
    // cross2d({-0.3, -0.4}, {1, 0}) = (-0.3)(0) - (-0.4)(1) = 0.4
    // torque = 0.4 * 20 = 8.0 (positive = CCW)
    CHECK_THAT(torq, WithinAbs(8.0f, 1e-4f));
}

TEST_CASE("Thruster rotates with body", "[thruster]") {
    // Rear thruster pointing +Y, but body rotated 90 degrees CCW
    Thruster t;
    t.local_position = {0, -0.5f};
    t.local_direction = {0, 1};
    t.max_thrust = 50.0f;
    t.power = 1.0f;

    float angle = std::numbers::pi_v<float> / 2; // 90 degrees CCW
    Vec2 force = t.world_force(angle);

    // +Y rotated 90 CCW = -X direction
    CHECK_THAT(force.x, WithinAbs(-50.0f, 1e-3f));
    CHECK_THAT(force.y, WithinAbs(0.0f, 1e-3f));
}

TEST_CASE("Equal opposite side thrusters cancel rotation", "[thruster]") {
    // Left and right rear thrusters firing equally
    Thruster left;
    left.local_position = {-0.3f, -0.4f};
    left.local_direction = {1, 0};
    left.max_thrust = 20.0f;
    left.power = 1.0f;

    Thruster right;
    right.local_position = {0.3f, -0.4f};
    right.local_direction = {-1, 0};
    right.max_thrust = 20.0f;
    right.power = 1.0f;

    float total_torque = left.torque(0) + right.torque(0);
    CHECK_THAT(total_torque, WithinAbs(0.0f, 1e-5f));
}
