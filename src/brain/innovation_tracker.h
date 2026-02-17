#pragma once

#include <map>
#include <utility>

// Tracks innovation numbers for structural mutations within a generation.
// Same source→target mutation in the same generation gets the same innovation number.
// Call new_generation() between generations to reset the cache.
class InnovationTracker {
public:
    explicit InnovationTracker(int start_innovation = 1)
        : next_innovation_(start_innovation) {}

    // Get an existing innovation number for this source→target pair (if seen this
    // generation), or assign a new one.
    int get_or_create(int source_node, int target_node);

    // Reset the per-generation cache. Future identical mutations get new numbers.
    void new_generation();

    int next_innovation() const { return next_innovation_; }

private:
    int next_innovation_;
    std::map<std::pair<int,int>, int> this_gen_cache_;
};
