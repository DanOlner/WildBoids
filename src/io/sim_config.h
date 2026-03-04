#pragma once

#include "simulation/world.h"
#include "simulation/morphology_genome.h"
#include "brain/population.h"
#include <string>

enum class FitnessMode {
    Gross,  // total_energy_gained (cumulative intake, never decreases)
    Net     // total_energy_gained - total_energy_spent (rewards efficiency)
};

struct SimConfig {
    WorldConfig world;
    PopulationParams neat;

    // Evolution run parameters (only used by headless runner)
    int ticks_per_generation = 2400;
    int generations = 100;
    int save_interval = 10;
    FitnessMode fitness_mode = FitnessMode::Gross;

    // Morphology evolution (eye position/arc evolution)
    MorphologyEvolutionConfig morphology;
};

// Load simulation config from a JSON file. Throws on parse/validation error.
SimConfig load_sim_config(const std::string& path);
