#pragma once

#include "simulation/vec2.h"
#include "simulation/boid.h"
#include <string>
#include <vector>

struct ThrusterSpec {
    int id = 0;
    std::string label;
    Vec2 position;
    Vec2 direction;
    float max_thrust = 0;
};

struct BoidSpec {
    std::string version;
    std::string type;           // "prey" or "predator"
    float mass = 1.0f;
    float moment_of_inertia = 1.0f;
    float initial_energy = 100.0f;
    std::vector<ThrusterSpec> thrusters;
};

// Load a boid spec from a JSON file. Throws on parse/validation error.
BoidSpec load_boid_spec(const std::string& path);

// Create a Boid from a spec (positioned at origin, zero velocity).
Boid create_boid_from_spec(const BoidSpec& spec);

// Save a boid spec to a JSON file.
void save_boid_spec(const BoidSpec& spec, const std::string& path);
