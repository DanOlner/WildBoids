#include "brain/neat_network.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_set>

NeatNetwork::NeatNetwork(const NeatGenome& genome) {
    // Build node array and id-to-index mapping
    for (int i = 0; i < static_cast<int>(genome.nodes.size()); ++i) {
        const auto& ng = genome.nodes[i];
        RuntimeNode rn;
        rn.bias = ng.bias;
        rn.activation = ng.activation;
        nodes_.push_back(rn);
        id_to_index_[ng.id] = i;

        if (ng.type == NodeType::Input) {
            input_indices_.push_back(i);
            input_ids_.push_back(ng.id);
        } else if (ng.type == NodeType::Output) {
            output_indices_.push_back(i);
            output_ids_.push_back(ng.id);
        }
    }

    // Build connection array (enabled only)
    for (const auto& cg : genome.connections) {
        if (!cg.enabled) continue;

        auto src_it = id_to_index_.find(cg.source);
        auto tgt_it = id_to_index_.find(cg.target);
        if (src_it == id_to_index_.end() || tgt_it == id_to_index_.end()) continue;

        connections_.push_back({src_it->second, tgt_it->second, cg.weight});
    }

    // Build per-node incoming connection lists
    incoming_.resize(nodes_.size());
    for (const auto& c : connections_) {
        incoming_[c.target_idx].push_back(c);
    }

    build_eval_order();
}

void NeatNetwork::build_eval_order() {
    int n = static_cast<int>(nodes_.size());

    // Compute in-degree for non-input nodes (from enabled connections only)
    std::vector<int> in_degree(n, 0);
    std::vector<std::vector<int>> dependents(n); // node idx → list of nodes it feeds into

    // Track which nodes are inputs (they have no in-degree by definition)
    std::unordered_set<int> input_set(input_indices_.begin(), input_indices_.end());

    for (const auto& c : connections_) {
        if (!input_set.count(c.target_idx)) {
            in_degree[c.target_idx]++;
        }
        dependents[c.source_idx].push_back(c.target_idx);
    }

    // Kahn's algorithm: start with input nodes + any non-input nodes with zero in-degree
    std::queue<int> ready;
    for (int idx : input_indices_) {
        ready.push(idx);
    }
    // Also add any non-input nodes with no incoming connections
    for (int i = 0; i < n; ++i) {
        if (!input_set.count(i) && in_degree[i] == 0) {
            ready.push(i);
        }
    }

    eval_order_.clear();
    std::unordered_set<int> visited;

    while (!ready.empty()) {
        int curr = ready.front();
        ready.pop();

        if (visited.count(curr)) continue;
        visited.insert(curr);

        // Only add non-input nodes to eval order
        if (!input_set.count(curr)) {
            eval_order_.push_back(curr);
        }

        for (int dep : dependents[curr]) {
            in_degree[dep]--;
            if (in_degree[dep] <= 0 && !visited.count(dep)) {
                ready.push(dep);
            }
        }
    }
}

void NeatNetwork::activate(const float* inputs, int n_in,
                            float* outputs, int n_out) {
    // Load input values (Linear activation = pass-through)
    int actual_in = std::min(n_in, static_cast<int>(input_indices_.size()));
    for (int i = 0; i < actual_in; ++i) {
        nodes_[input_indices_[i]].value = inputs[i];
    }
    // Zero any unset input nodes
    for (int i = actual_in; i < static_cast<int>(input_indices_.size()); ++i) {
        nodes_[input_indices_[i]].value = 0.0f;
    }

    // Evaluate each node in topological order: accumulate inputs then activate.
    // This ensures hidden node values are computed before downstream nodes read them.
    for (int idx : eval_order_) {
        float sum = nodes_[idx].bias;
        for (const auto& c : incoming_[idx]) {
            sum += nodes_[c.source_idx].value * c.weight;
        }
        nodes_[idx].value = apply_activation(nodes_[idx].activation, sum);
    }

    // Read output values
    int actual_out = std::min(n_out, static_cast<int>(output_indices_.size()));
    for (int i = 0; i < actual_out; ++i) {
        outputs[i] = nodes_[output_indices_[i]].value;
    }
    // Zero extra outputs
    for (int i = actual_out; i < n_out; ++i) {
        outputs[i] = 0.0f;
    }
}

void NeatNetwork::reset() {
    for (auto& node : nodes_) {
        node.value = 0.0f;
        node.incoming_sum = 0.0f;
    }
}

float NeatNetwork::apply_activation(ActivationFn fn, float x) {
    switch (fn) {
        case ActivationFn::Sigmoid:
            return 1.0f / (1.0f + std::exp(-x));
        case ActivationFn::Tanh:
            return std::tanh(x);
        case ActivationFn::ReLU:
            return std::max(0.0f, x);
        case ActivationFn::Linear:
            return x;
    }
    return x;
}
