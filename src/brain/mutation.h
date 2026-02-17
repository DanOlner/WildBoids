#pragma once

#include "brain/neat_genome.h"
#include "brain/innovation_tracker.h"
#include <random>

// Perturb or replace connection weights.
// Each weight has perturb_prob chance of being perturbed (Gaussian noise with given sigma),
// and replace_prob chance of being replaced with a fresh random value from U(-2, 2).
void mutate_weights(NeatGenome& genome, std::mt19937& rng,
                    float perturb_prob = 0.8f, float sigma = 0.3f,
                    float replace_prob = 0.1f);

// Add a new connection between two previously unconnected nodes.
// Returns true if a connection was added, false if the genome is already fully connected
// or no valid connection could be found after max_attempts tries.
bool mutate_add_connection(NeatGenome& genome, std::mt19937& rng,
                           InnovationTracker& tracker,
                           int max_attempts = 20);

// Split an existing enabled connection by inserting a hidden node.
// The original connection is disabled. Two new connections are created:
//   source → new_node (weight 1.0) and new_node → target (original weight).
// This preserves existing behaviour before further mutation.
// Returns true if a node was added, false if no enabled connections exist.
bool mutate_add_node(NeatGenome& genome, std::mt19937& rng,
                     InnovationTracker& tracker);

// Toggle enable/disable on a random connection.
void mutate_toggle_connection(NeatGenome& genome, std::mt19937& rng);

// Delete a random connection. If this orphans a hidden node (no remaining connections
// to or from it), the orphaned node is also removed.
// Returns true if a connection was deleted, false if no connections exist.
bool mutate_delete_connection(NeatGenome& genome, std::mt19937& rng);
