#include "brain/mutation.h"
#include <algorithm>
#include <set>
#include <unordered_set>

void mutate_weights(NeatGenome& genome, std::mt19937& rng,
                    float perturb_prob, float sigma, float replace_prob) {
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);
    std::normal_distribution<float> perturb(0.0f, sigma);
    std::uniform_real_distribution<float> fresh(-2.0f, 2.0f);

    for (auto& c : genome.connections) {
        float r = coin(rng);
        if (r < replace_prob) {
            c.weight = fresh(rng);
        } else if (r < replace_prob + perturb_prob) {
            c.weight += perturb(rng);
        }
        // else: leave unchanged
    }
}

bool mutate_add_connection(NeatGenome& genome, std::mt19937& rng,
                           InnovationTracker& tracker,
                           int max_attempts) {
    if (genome.nodes.size() < 2) return false;

    // Build set of existing connections for quick lookup
    std::set<std::pair<int,int>> existing;
    for (const auto& c : genome.connections) {
        existing.insert({c.source, c.target});
    }

    std::uniform_int_distribution<int> node_dist(0, static_cast<int>(genome.nodes.size()) - 1);

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        int si = node_dist(rng);
        int ti = node_dist(rng);
        if (si == ti) continue;

        const auto& src = genome.nodes[si];
        const auto& tgt = genome.nodes[ti];

        // Don't connect output → input (wrong direction for feed-forward)
        // Don't connect to an input node or from an output node
        if (tgt.type == NodeType::Input) continue;
        if (src.type == NodeType::Output && tgt.type == NodeType::Output) continue;

        // Check for existing connection in either direction
        if (existing.count({src.id, tgt.id})) continue;

        // For feed-forward: don't create cycles.
        // Simple heuristic: inputs can connect to hidden/output,
        // hidden can connect to hidden/output, output can't be a source
        // (unless connecting to hidden, which we allow for now — topological sort handles it)
        if (src.type == NodeType::Output) continue;

        int innov = tracker.get_or_create(src.id, tgt.id);
        genome.connections.push_back({innov, src.id, tgt.id, 0.0f, true});
        return true;
    }

    return false;
}

bool mutate_add_node(NeatGenome& genome, std::mt19937& rng,
                     InnovationTracker& tracker) {
    // Collect indices of enabled connections
    std::vector<int> enabled_indices;
    for (int i = 0; i < static_cast<int>(genome.connections.size()); ++i) {
        if (genome.connections[i].enabled) {
            enabled_indices.push_back(i);
        }
    }
    if (enabled_indices.empty()) return false;

    // Pick a random enabled connection to split
    std::uniform_int_distribution<int> dist(0, static_cast<int>(enabled_indices.size()) - 1);
    int ci = enabled_indices[dist(rng)];

    // Copy values before modifying the vector (push_back can reallocate and
    // invalidate references into genome.connections)
    int src_id = genome.connections[ci].source;
    int tgt_id = genome.connections[ci].target;
    float orig_weight = genome.connections[ci].weight;

    // Disable the original connection
    genome.connections[ci].enabled = false;

    // Create a new hidden node with the next available id
    int max_id = 0;
    for (const auto& n : genome.nodes) {
        max_id = std::max(max_id, n.id);
    }
    int new_id = max_id + 1;

    genome.nodes.push_back({new_id, NodeType::Hidden, ActivationFn::Sigmoid, 0.0f});

    // source → new_node with weight 1.0 (preserves signal magnitude)
    int innov1 = tracker.get_or_create(src_id, new_id);
    genome.connections.push_back({innov1, src_id, new_id, 1.0f, true});

    // new_node → target with original weight (preserves existing behaviour)
    int innov2 = tracker.get_or_create(new_id, tgt_id);
    genome.connections.push_back({innov2, new_id, tgt_id, orig_weight, true});

    return true;
}

void mutate_toggle_connection(NeatGenome& genome, std::mt19937& rng) {
    if (genome.connections.empty()) return;
    std::uniform_int_distribution<int> dist(0, static_cast<int>(genome.connections.size()) - 1);
    auto& c = genome.connections[dist(rng)];
    c.enabled = !c.enabled;
}

bool mutate_delete_connection(NeatGenome& genome, std::mt19937& rng) {
    if (genome.connections.empty()) return false;

    std::uniform_int_distribution<int> dist(0, static_cast<int>(genome.connections.size()) - 1);
    int ci = dist(rng);
    genome.connections.erase(genome.connections.begin() + ci);

    // Clean up orphaned hidden nodes (nodes with no remaining connections)
    std::unordered_set<int> connected_nodes;
    for (const auto& c : genome.connections) {
        connected_nodes.insert(c.source);
        connected_nodes.insert(c.target);
    }

    genome.nodes.erase(
        std::remove_if(genome.nodes.begin(), genome.nodes.end(),
            [&](const NodeGene& n) {
                return n.type == NodeType::Hidden && !connected_nodes.count(n.id);
            }),
        genome.nodes.end());

    return true;
}
