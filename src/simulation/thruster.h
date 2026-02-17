#pragma once

#include "simulation/vec2.h"

struct Thruster {
    Vec2 local_position;    // offset from center of mass (body frame)
    Vec2 local_direction;   // direction it fires (body frame, normalised)
    float max_thrust = 0;   // maximum force magnitude
    float power = 0;        // current power level [0, 1]

    // Compute world-frame force given the body's current heading
    Vec2 world_force(float body_angle) const {
        return local_direction.rotated(body_angle) * (power * max_thrust);
    }

    // Compute torque about center of mass
    // Both vectors in body frame — cross product is rotation-invariant
    float torque(float /*body_angle*/) const {
        return cross2d(local_position, local_direction) * (power * max_thrust);
    }
};
