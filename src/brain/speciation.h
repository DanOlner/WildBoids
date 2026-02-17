#pragma once

#include "brain/neat_genome.h"
#include <vector>

// Parameters for compatibility distance calculation
struct CompatibilityParams {
    float c1 = 1.0f;  // coefficient for excess genes
    float c2 = 1.0f;  // coefficient for disjoint genes
    float c3 = 0.4f;  // coefficient for mean weight difference
    int normalise_threshold = 20;  // N = max(genome_size, this) to avoid penalising small genomes
};

// Compute NEAT compatibility distance between two genomes.
// δ = (c1 * E / N) + (c2 * D / N) + c3 * W̄
// E = excess genes, D = disjoint genes, W̄ = mean weight diff of matching genes,
// N = max(size of larger genome, normalise_threshold).
float compatibility_distance(const NeatGenome& a, const NeatGenome& b,
                             const CompatibilityParams& params);

// A species: a group of genomes with a representative
struct Species {
    int id;
    NeatGenome representative;  // from previous generation, used for membership testing
    std::vector<int> members;   // indices into the population's genome list
    float best_fitness = 0.0f;
    int stagnation_count = 0;   // generations without improvement
};

// Assign genomes to species based on compatibility distance.
// Updates species list in-place: clears member lists, assigns each genome,
// creates new species for unmatched genomes, removes empty species.
void assign_species(std::vector<Species>& species,
                    const std::vector<NeatGenome>& genomes,
                    const CompatibilityParams& params,
                    float threshold,
                    int& next_species_id);
