#pragma once

#include "simulation/rigid_body.h"
#include "simulation/thruster.h"
#include "simulation/sensory_system.h"
#include "brain/processing_network.h"
#include <vector>
#include <string>
#include <optional>
#include <memory>

struct Boid {
    std::string type;   // "prey" or "predator"
    RigidBody body;
    std::vector<Thruster> thrusters;
    float energy = 100.0f;

    // Sensory system (optional — boids without sensors still work)
    std::optional<SensorySystem> sensors;
    std::vector<float> sensor_outputs;

    // Brain (optional — boids without a brain have thrusters set externally)
    std::unique_ptr<ProcessingNetwork> brain;

    // Apply thruster forces to rigid body and integrate one timestep.
    void step(float dt, float linear_drag, float angular_drag);
};
