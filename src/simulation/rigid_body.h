#pragma once

#include "simulation/vec2.h"

struct RigidBody {
    Vec2 position{0, 0};
    Vec2 velocity{0, 0};
    float angle = 0;            // heading in radians, CCW positive
    float angular_velocity = 0; // rad/s, CCW positive
    float mass = 1.0f;
    float moment_of_inertia = 1.0f;

    void apply_force(Vec2 force);
    void apply_torque(float torque);
    void integrate(float dt, float linear_drag, float angular_drag);
    void clear_forces();

private:
    Vec2 net_force{0, 0};
    float net_torque = 0;
};
