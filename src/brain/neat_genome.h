#pragma once

#include <vector>

enum class NodeType { Input, Output, Hidden };
enum class ActivationFn { Sigmoid, Tanh, ReLU, Linear };

struct NodeGene {
    int id;
    NodeType type;
    ActivationFn activation = ActivationFn::Sigmoid;
    float bias = 0.0f;
};

struct ConnectionGene {
    int innovation;
    int source;
    int target;
    float weight = 0.0f;
    bool enabled = true;
};

struct NeatGenome {
    std::vector<NodeGene> nodes;
    std::vector<ConnectionGene> connections;

    // Create minimal topology: all inputs connected to all outputs, no hidden nodes.
    // Input nodes get ids [0, n_inputs), output nodes get ids [n_inputs, n_inputs+n_outputs).
    // next_innovation is incremented for each connection created.
    static NeatGenome minimal(int n_inputs, int n_outputs, int& next_innovation);
};
