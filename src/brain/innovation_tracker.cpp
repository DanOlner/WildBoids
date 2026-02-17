#include "brain/innovation_tracker.h"

int InnovationTracker::get_or_create(int source_node, int target_node) {
    auto key = std::make_pair(source_node, target_node);
    auto it = this_gen_cache_.find(key);
    if (it != this_gen_cache_.end()) {
        return it->second;
    }
    int innov = next_innovation_++;
    this_gen_cache_[key] = innov;
    return innov;
}

void InnovationTracker::new_generation() {
    this_gen_cache_.clear();
}
