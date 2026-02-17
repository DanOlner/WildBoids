#pragma once

#include "simulation/world.h"
#include "brain/population.h"
#include <string>

struct SimConfig {
    WorldConfig world;
    PopulationParams neat;

    // Evolution run parameters (only used by headless runner)
    int ticks_per_generation = 2400;
    int generations = 100;
    int save_interval = 10;
};

// Load simulation config from a JSON file. Throws on parse/validation error.
SimConfig load_sim_config(const std::string& path);
