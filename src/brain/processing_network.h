#pragma once

class ProcessingNetwork {
public:
    virtual ~ProcessingNetwork() = default;

    // Read n_in floats from inputs, write n_out floats to outputs.
    // Output values should be in [0, 1] for thruster power levels.
    virtual void activate(const float* inputs, int n_in,
                          float* outputs, int n_out) = 0;

    virtual void reset() = 0;
};
