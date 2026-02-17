#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "brain/neat_network.h"
#include "brain/direct_wire_network.h"
#include <cmath>

using Catch::Matchers::WithinAbs;

static float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

TEST_CASE("NeatNetwork: minimal genome, zero weights → sigmoid(0) outputs", "[neat_network]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(3, 2, next_innov);
    NeatNetwork net(g);

    float inputs[] = {1.0f, 0.5f, 0.0f};
    float outputs[2] = {};
    net.activate(inputs, 3, outputs, 2);

    // All weights zero, no bias → sigmoid(0) = 0.5
    CHECK_THAT(outputs[0], WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(outputs[1], WithinAbs(0.5f, 1e-6f));
}

TEST_CASE("NeatNetwork: single large positive weight → output near 1", "[neat_network]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(2, 1, next_innov);
    // Set weight on first connection (input 0 → output 0) to large positive
    g.connections[0].weight = 10.0f;

    NeatNetwork net(g);

    float inputs[] = {1.0f, 0.0f};
    float output = 0.0f;
    net.activate(inputs, 2, &output, 1);

    CHECK(output > 0.999f);
}

TEST_CASE("NeatNetwork: single large negative weight → output near 0", "[neat_network]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(2, 1, next_innov);
    g.connections[0].weight = -10.0f;

    NeatNetwork net(g);

    float inputs[] = {1.0f, 0.0f};
    float output = 0.0f;
    net.activate(inputs, 2, &output, 1);

    CHECK(output < 0.001f);
}

TEST_CASE("NeatNetwork: matches DirectWireNetwork for minimal genome", "[neat_network]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(3, 2, next_innov);

    // Set some weights
    // Connection layout: input 0→out 0, input 0→out 1, input 1→out 0, input 1→out 1, input 2→out 0, input 2→out 1
    g.connections[0].weight = 1.0f;   // in0 → out0
    g.connections[1].weight = -0.5f;  // in0 → out1
    g.connections[2].weight = 2.0f;   // in1 → out0
    g.connections[3].weight = 0.3f;   // in1 → out1
    g.connections[4].weight = -1.0f;  // in2 → out0
    g.connections[5].weight = 0.7f;   // in2 → out1

    NeatNetwork neat_net(g);

    // Build equivalent DirectWireNetwork
    DirectWireNetwork dw_net(3, 2);
    dw_net.set_weight(0, 0, 1.0f);
    dw_net.set_weight(0, 1, -0.5f);
    dw_net.set_weight(1, 0, 2.0f);
    dw_net.set_weight(1, 1, 0.3f);
    dw_net.set_weight(2, 0, -1.0f);
    dw_net.set_weight(2, 1, 0.7f);

    float inputs[] = {0.8f, 0.3f, 0.6f};
    float neat_out[2] = {}, dw_out[2] = {};

    neat_net.activate(inputs, 3, neat_out, 2);
    dw_net.activate(inputs, 3, dw_out, 2);

    CHECK_THAT(neat_out[0], WithinAbs(dw_out[0], 1e-5f));
    CHECK_THAT(neat_out[1], WithinAbs(dw_out[1], 1e-5f));
}

TEST_CASE("NeatNetwork: disabled connection is ignored", "[neat_network]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(1, 1, next_innov);
    g.connections[0].weight = 10.0f;
    g.connections[0].enabled = false;

    NeatNetwork net(g);

    float input = 1.0f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);

    // Connection disabled → output = sigmoid(0) = 0.5
    CHECK_THAT(output, WithinAbs(0.5f, 1e-6f));
}

TEST_CASE("NeatNetwork: output node bias works", "[neat_network]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(1, 1, next_innov);
    // Set bias on the output node (node id 1, index 1)
    g.nodes[1].bias = 3.0f;

    NeatNetwork net(g);

    float input = 0.0f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);

    CHECK_THAT(output, WithinAbs(sigmoid(3.0f), 1e-5f));
}

TEST_CASE("NeatNetwork: one hidden node (input → hidden → output)", "[neat_network]") {
    // Manually build a genome: 1 input, 1 hidden, 1 output
    NeatGenome g;
    g.nodes.push_back({0, NodeType::Input, ActivationFn::Linear, 0.0f});
    g.nodes.push_back({1, NodeType::Output, ActivationFn::Sigmoid, 0.0f});
    g.nodes.push_back({2, NodeType::Hidden, ActivationFn::Tanh, 0.0f});

    // input(0) → hidden(2), weight = 1.0
    g.connections.push_back({1, 0, 2, 1.0f, true});
    // hidden(2) → output(1), weight = 2.0
    g.connections.push_back({2, 2, 1, 2.0f, true});

    NeatNetwork net(g);

    float input = 0.5f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);

    // Hidden: tanh(0.5 * 1.0) = tanh(0.5)
    float hidden_val = std::tanh(0.5f);
    // Output: sigmoid(hidden_val * 2.0)
    float expected = sigmoid(hidden_val * 2.0f);

    CHECK_THAT(output, WithinAbs(expected, 1e-5f));
}

TEST_CASE("NeatNetwork: two hidden nodes in chain", "[neat_network]") {
    // input(0) → hidden_a(2) → hidden_b(3) → output(1)
    NeatGenome g;
    g.nodes.push_back({0, NodeType::Input, ActivationFn::Linear, 0.0f});
    g.nodes.push_back({1, NodeType::Output, ActivationFn::Sigmoid, 0.0f});
    g.nodes.push_back({2, NodeType::Hidden, ActivationFn::Tanh, 0.0f});
    g.nodes.push_back({3, NodeType::Hidden, ActivationFn::ReLU, 0.0f});

    g.connections.push_back({1, 0, 2, 1.5f, true});  // in → hidden_a
    g.connections.push_back({2, 2, 3, -1.0f, true});  // hidden_a → hidden_b
    g.connections.push_back({3, 3, 1, 2.0f, true});   // hidden_b → output

    NeatNetwork net(g);

    float input = 1.0f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);

    float ha = std::tanh(1.0f * 1.5f);          // tanh(1.5)
    float hb = std::max(0.0f, ha * -1.0f);      // ReLU(tanh(1.5) * -1) = ReLU(negative) = 0
    float expected = sigmoid(hb * 2.0f);         // sigmoid(0) = 0.5

    CHECK_THAT(output, WithinAbs(expected, 1e-5f));
}

TEST_CASE("NeatNetwork: diamond topology (two paths to output)", "[neat_network]") {
    // input(0) → hidden_a(2) → output(1)
    //         → hidden_b(3) → output(1)
    NeatGenome g;
    g.nodes.push_back({0, NodeType::Input, ActivationFn::Linear, 0.0f});
    g.nodes.push_back({1, NodeType::Output, ActivationFn::Sigmoid, 0.0f});
    g.nodes.push_back({2, NodeType::Hidden, ActivationFn::Tanh, 0.0f});
    g.nodes.push_back({3, NodeType::Hidden, ActivationFn::Tanh, 0.0f});

    g.connections.push_back({1, 0, 2, 1.0f, true});   // in → ha
    g.connections.push_back({2, 0, 3, -1.0f, true});   // in → hb
    g.connections.push_back({3, 2, 1, 1.0f, true});    // ha → out
    g.connections.push_back({4, 3, 1, 1.0f, true});    // hb → out

    NeatNetwork net(g);

    float input = 0.5f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);

    float ha = std::tanh(0.5f);
    float hb = std::tanh(-0.5f);
    float expected = sigmoid(ha + hb); // tanh is odd: tanh(x) + tanh(-x) = 0

    CHECK_THAT(output, WithinAbs(expected, 1e-5f));
    // tanh(0.5) + tanh(-0.5) ≈ 0, so output ≈ sigmoid(0) = 0.5
    CHECK_THAT(output, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("NeatNetwork: reset clears node values", "[neat_network]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(1, 1, next_innov);
    g.connections[0].weight = 5.0f;

    NeatNetwork net(g);

    float input = 1.0f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);
    CHECK(output > 0.99f);

    net.reset();

    // After reset, activate with zero input
    input = 0.0f;
    net.activate(&input, 1, &output, 1);
    CHECK_THAT(output, WithinAbs(0.5f, 1e-6f));
}

TEST_CASE("NeatNetwork: polymorphic use via ProcessingNetwork", "[neat_network]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(2, 2, next_innov);
    g.connections[0].weight = 3.0f; // in0 → out0

    auto net = std::make_unique<NeatNetwork>(g);
    ProcessingNetwork* brain = net.get();

    float inputs[] = {1.0f, 0.0f};
    float outputs[2] = {};
    brain->activate(inputs, 2, outputs, 2);

    CHECK_THAT(outputs[0], WithinAbs(sigmoid(3.0f), 1e-5f));
    CHECK_THAT(outputs[1], WithinAbs(0.5f, 1e-6f));
}
