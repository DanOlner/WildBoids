#include "brain/crossover.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

NeatGenome crossover(const NeatGenome& fitter_parent,
                     const NeatGenome& other_parent,
                     std::mt19937& rng,
                     float disable_prob) {
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);

    // Index other parent's connections by innovation number
    std::unordered_map<int, const ConnectionGene*> other_conns;
    for (const auto& c : other_parent.connections) {
        other_conns[c.innovation] = &c;
    }

    NeatGenome offspring;

    // Inherit connections
    for (const auto& fc : fitter_parent.connections) {
        auto it = other_conns.find(fc.innovation);

        if (it != other_conns.end()) {
            // Matching gene: randomly pick from either parent
            const ConnectionGene& chosen = (coin(rng) < 0.5f) ? fc : *(it->second);
            ConnectionGene gene = chosen;

            // If disabled in either parent, disable_prob chance of being disabled
            bool disabled_in_either = !fc.enabled || !it->second->enabled;
            if (disabled_in_either) {
                gene.enabled = (coin(rng) >= disable_prob);
            }

            offspring.connections.push_back(gene);
        } else {
            // Disjoint/excess gene from fitter parent — always inherit
            offspring.connections.push_back(fc);
        }
    }

    // Collect all node IDs referenced by offspring connections
    std::unordered_set<int> needed_nodes;
    for (const auto& c : offspring.connections) {
        needed_nodes.insert(c.source);
        needed_nodes.insert(c.target);
    }

    // Also include all input and output nodes (they must always be present)
    for (const auto& n : fitter_parent.nodes) {
        if (n.type == NodeType::Input || n.type == NodeType::Output) {
            needed_nodes.insert(n.id);
        }
    }

    // Build node map from both parents (fitter parent takes priority)
    std::unordered_map<int, const NodeGene*> node_map;
    for (const auto& n : other_parent.nodes) {
        node_map[n.id] = &n;
    }
    for (const auto& n : fitter_parent.nodes) {
        node_map[n.id] = &n;  // overwrite with fitter parent's version
    }

    // Add needed nodes to offspring
    for (int id : needed_nodes) {
        auto it = node_map.find(id);
        if (it != node_map.end()) {
            offspring.nodes.push_back(*(it->second));
        }
    }

    // Sort nodes by id for consistent ordering
    std::sort(offspring.nodes.begin(), offspring.nodes.end(),
              [](const NodeGene& a, const NodeGene& b) { return a.id < b.id; });

    return offspring;
}
