#pragma once

#include "brain/neat_genome.h"
#include <random>

// NEAT crossover: combine two parent genomes into an offspring genome.
// Matching genes (same innovation number) are randomly inherited from either parent.
// Disjoint and excess genes are inherited from the fitter parent only.
// If a gene is disabled in either parent, it has disable_prob chance of being
// disabled in the offspring.
NeatGenome crossover(const NeatGenome& fitter_parent,
                     const NeatGenome& other_parent,
                     std::mt19937& rng,
                     float disable_prob = 0.75f);
