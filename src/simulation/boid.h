#pragma once

#include "simulation/rigid_body.h"
#include "simulation/thruster.h"
#include "simulation/sensory_system.h"
#include <vector>
#include <string>
#include <optional>

struct Boid {
    std::string type;   // "prey" or "predator"
    RigidBody body;
    std::vector<Thruster> thrusters;
    float energy = 100.0f;

    // Sensory system (optional — boids without sensors still work)
    std::optional<SensorySystem> sensors;
    std::vector<float> sensor_outputs;

    // Apply thruster forces to rigid body and integrate one timestep.
    // No brain yet — thruster power levels must be set externally.
    void step(float dt, float linear_drag, float angular_drag);
};
