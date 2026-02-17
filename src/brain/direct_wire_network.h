#pragma once

#include "brain/processing_network.h"
#include <vector>

// Test fixture: fixed weight matrix, no evolution.
// Maps inputs to outputs via: output[j] = sigmoid(bias[j] + sum_i(weight[i*n_out+j] * input[i]))
class DirectWireNetwork : public ProcessingNetwork {
public:
    DirectWireNetwork(int n_in, int n_out);

    void set_weight(int in, int out, float w);
    void set_bias(int out, float b);

    void activate(const float* inputs, int n_in,
                  float* outputs, int n_out) override;
    void reset() override {}

    int input_count() const { return n_in_; }
    int output_count() const { return n_out_; }

private:
    int n_in_;
    int n_out_;
    std::vector<float> weights_;  // n_in * n_out, row-major: weights_[i*n_out+j]
    std::vector<float> biases_;   // n_out

    static float sigmoid(float x);
};
