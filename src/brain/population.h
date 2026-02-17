#pragma once

#include "brain/neat_genome.h"
#include "brain/speciation.h"
#include "brain/innovation_tracker.h"
#include <vector>
#include <functional>
#include <random>

struct PopulationParams {
    int population_size = 150;

    // Mutation probabilities (per genome, each generation)
    float weight_mutate_prob = 0.8f;
    float add_connection_prob = 0.05f;
    float add_node_prob = 0.03f;
    float toggle_connection_prob = 0.01f;
    float delete_connection_prob = 0.01f;

    // Weight mutation parameters
    float weight_perturb_prob = 0.8f;
    float weight_sigma = 0.3f;
    float weight_replace_prob = 0.1f;

    // Crossover
    float crossover_prob = 0.75f;      // probability of crossover vs asexual reproduction
    float interspecies_prob = 0.001f;   // probability of interspecies crossover

    // Speciation
    CompatibilityParams compat;
    float compat_threshold = 3.0f;

    // Selection
    float survival_rate = 0.25f;  // top fraction of each species that breeds
    int elitism = 1;              // number of top genomes per species that survive unchanged

    // Stagnation
    int max_stagnation = 15;      // species removed after this many gens without improvement
};

class Population {
public:
    Population(const NeatGenome& seed, const PopulationParams& params,
               std::mt19937& rng);

    // Evaluate fitness of all genomes using the provided function.
    // The function receives a genome index and should return a fitness value.
    void evaluate(std::function<float(int genome_index, const NeatGenome& genome)> fitness_fn);

    // Advance to the next generation: speciate, select, reproduce.
    void advance_generation();

    // Accessors
    const NeatGenome& genome(int index) const { return genomes_[index]; }
    const std::vector<NeatGenome>& genomes() const { return genomes_; }
    int size() const { return static_cast<int>(genomes_.size()); }
    int generation() const { return generation_; }
    int species_count() const { return static_cast<int>(species_.size()); }
    const std::vector<Species>& species() const { return species_; }

    float fitness(int index) const { return fitness_[index]; }
    const NeatGenome& best_genome() const;
    float best_fitness() const;

    InnovationTracker& innovation_tracker() { return tracker_; }

private:
    PopulationParams params_;
    std::mt19937& rng_;
    InnovationTracker tracker_;

    std::vector<NeatGenome> genomes_;
    std::vector<float> fitness_;
    std::vector<Species> species_;
    int generation_ = 0;
    int next_species_id_ = 1;

    // Select a parent from a species by tournament selection
    int tournament_select(const Species& species, int k = 2);

    // Produce one offspring from a species
    NeatGenome reproduce_from_species(const Species& species);

    // Apply mutation to a genome
    void mutate(NeatGenome& genome);
};
