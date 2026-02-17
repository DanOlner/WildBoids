#pragma once

#include "brain/processing_network.h"
#include "brain/neat_genome.h"
#include <vector>
#include <unordered_map>

// Feed-forward NEAT network built from a NeatGenome.
// Nodes are evaluated in topological order; cycles are not supported
// (recurrent connections would need a different activation strategy).
class NeatNetwork : public ProcessingNetwork {
public:
    explicit NeatNetwork(const NeatGenome& genome);

    void activate(const float* inputs, int n_in,
                  float* outputs, int n_out) override;
    void reset() override;

    int input_count() const { return static_cast<int>(input_ids_.size()); }
    int output_count() const { return static_cast<int>(output_ids_.size()); }

private:
    struct RuntimeNode {
        float bias = 0.0f;
        ActivationFn activation = ActivationFn::Sigmoid;
        float value = 0.0f;
        float incoming_sum = 0.0f;
    };

    struct RuntimeConnection {
        int source_idx; // index into nodes_
        int target_idx; // index into nodes_
        float weight;
    };

    std::vector<RuntimeNode> nodes_;
    std::vector<RuntimeConnection> connections_;
    std::vector<int> input_indices_;   // indices into nodes_ for input nodes
    std::vector<int> output_indices_;  // indices into nodes_ for output nodes
    std::vector<int> eval_order_;      // topological order (hidden + output indices)

    // Per-node incoming connections (indexed by node index)
    std::vector<std::vector<RuntimeConnection>> incoming_;

    // Node id → index in nodes_
    std::unordered_map<int, int> id_to_index_;

    // Maps node gene ids to internal indices
    std::vector<int> input_ids_;  // original node ids (for counting)
    std::vector<int> output_ids_;

    void build_eval_order();
    static float apply_activation(ActivationFn fn, float x);
};
