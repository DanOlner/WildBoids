#pragma once

#include "simulation/rigid_body.h"
#include "simulation/thruster.h"
#include <vector>
#include <string>

struct Boid {
    std::string type;   // "prey" or "predator"
    RigidBody body;
    std::vector<Thruster> thrusters;
    float energy = 100.0f;

    // Apply thruster forces to rigid body and integrate one timestep.
    // No brain yet — thruster power levels must be set externally.
    void step(float dt, float linear_drag, float angular_drag);
};
