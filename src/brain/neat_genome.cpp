#include "brain/neat_genome.h"

NeatGenome NeatGenome::minimal(int n_inputs, int n_outputs, int& next_innovation) {
    NeatGenome genome;

    // Input nodes: ids [0, n_inputs)
    for (int i = 0; i < n_inputs; ++i) {
        genome.nodes.push_back({i, NodeType::Input, ActivationFn::Linear, 0.0f});
    }

    // Output nodes: ids [n_inputs, n_inputs + n_outputs)
    // Sigmoid activation so outputs are naturally [0, 1] for thruster commands
    for (int i = 0; i < n_outputs; ++i) {
        genome.nodes.push_back({n_inputs + i, NodeType::Output, ActivationFn::Sigmoid, 0.0f});
    }

    // Fully connected: every input → every output
    for (int i = 0; i < n_inputs; ++i) {
        for (int o = 0; o < n_outputs; ++o) {
            genome.connections.push_back({
                next_innovation++,
                i,                  // source (input node id)
                n_inputs + o,       // target (output node id)
                0.0f,               // weight starts at zero
                true                // enabled
            });
        }
    }

    return genome;
}
