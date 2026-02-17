#include "brain/direct_wire_network.h"
#include <cmath>
#include <algorithm>

DirectWireNetwork::DirectWireNetwork(int n_in, int n_out)
    : n_in_(n_in), n_out_(n_out),
      weights_(n_in * n_out, 0.0f),
      biases_(n_out, 0.0f) {}

void DirectWireNetwork::set_weight(int in, int out, float w) {
    weights_[in * n_out_ + out] = w;
}

void DirectWireNetwork::set_bias(int out, float b) {
    biases_[out] = b;
}

void DirectWireNetwork::activate(const float* inputs, int n_in,
                                  float* outputs, int n_out) {
    int actual_in = std::min(n_in, n_in_);
    int actual_out = std::min(n_out, n_out_);

    for (int j = 0; j < actual_out; ++j) {
        float sum = biases_[j];
        for (int i = 0; i < actual_in; ++i) {
            sum += weights_[i * n_out_ + j] * inputs[i];
        }
        outputs[j] = sigmoid(sum);
    }

    // Zero any extra outputs beyond what we compute
    for (int j = actual_out; j < n_out; ++j) {
        outputs[j] = 0.0f;
    }
}

float DirectWireNetwork::sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}
