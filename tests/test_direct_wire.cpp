#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "brain/direct_wire_network.h"
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

static float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

TEST_CASE("DirectWire: all-zero weights produce sigmoid(0) outputs", "[brain]") {
    DirectWireNetwork net(3, 2);

    float inputs[] = {1.0f, 0.5f, 0.0f};
    float outputs[2] = {};
    net.activate(inputs, 3, outputs, 2);

    // All weights and biases zero → sigmoid(0) = 0.5
    CHECK_THAT(outputs[0], WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(outputs[1], WithinAbs(0.5f, 1e-6f));
}

TEST_CASE("DirectWire: single weight drives one output", "[brain]") {
    DirectWireNetwork net(2, 2);
    net.set_weight(0, 0, 5.0f); // input 0 → output 0

    float inputs[] = {1.0f, 0.0f};
    float outputs[2] = {};
    net.activate(inputs, 2, outputs, 2);

    CHECK_THAT(outputs[0], WithinAbs(sigmoid(5.0f), 1e-5f));
    CHECK_THAT(outputs[1], WithinAbs(0.5f, 1e-6f)); // no weight → sigmoid(0)
}

TEST_CASE("DirectWire: large positive weight saturates toward 1", "[brain]") {
    DirectWireNetwork net(1, 1);
    net.set_weight(0, 0, 10.0f);

    float input = 1.0f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);

    CHECK(output > 0.999f);
}

TEST_CASE("DirectWire: large negative weight saturates toward 0", "[brain]") {
    DirectWireNetwork net(1, 1);
    net.set_weight(0, 0, -10.0f);

    float input = 1.0f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);

    CHECK(output < 0.001f);
}

TEST_CASE("DirectWire: bias shifts output", "[brain]") {
    DirectWireNetwork net(1, 1);
    net.set_bias(0, 3.0f);

    float input = 0.0f;
    float output = 0.0f;
    net.activate(&input, 1, &output, 1);

    CHECK_THAT(output, WithinAbs(sigmoid(3.0f), 1e-5f));
}

TEST_CASE("DirectWire: multiple inputs sum correctly", "[brain]") {
    DirectWireNetwork net(3, 1);
    net.set_weight(0, 0, 1.0f);
    net.set_weight(1, 0, 2.0f);
    net.set_weight(2, 0, -1.0f);

    float inputs[] = {1.0f, 0.5f, 1.0f};
    float output = 0.0f;
    net.activate(inputs, 3, &output, 1);

    // 1*1 + 0.5*2 + 1*(-1) = 1.0
    CHECK_THAT(output, WithinAbs(sigmoid(1.0f), 1e-5f));
}

TEST_CASE("DirectWire: more sensors than thrusters (extra inputs ignored)", "[brain]") {
    DirectWireNetwork net(2, 1);
    net.set_weight(0, 0, 1.0f);
    net.set_weight(1, 0, 1.0f);

    // Pass 4 inputs but net only has 2 input slots
    float inputs[] = {1.0f, 1.0f, 99.0f, 99.0f};
    float output = 0.0f;
    net.activate(inputs, 4, &output, 1);

    // Only first 2 inputs used: 1+1 = 2
    CHECK_THAT(output, WithinAbs(sigmoid(2.0f), 1e-5f));
}

TEST_CASE("DirectWire: fewer sensors than thrusters (extra outputs zeroed)", "[brain]") {
    DirectWireNetwork net(1, 2);
    net.set_weight(0, 0, 1.0f);
    net.set_weight(0, 1, -1.0f);

    float input = 1.0f;
    float outputs[3] = {-1.0f, -1.0f, -1.0f};
    // Request 3 outputs but net only has 2
    net.activate(&input, 1, outputs, 3);

    CHECK_THAT(outputs[0], WithinAbs(sigmoid(1.0f), 1e-5f));
    CHECK_THAT(outputs[1], WithinAbs(sigmoid(-1.0f), 1e-5f));
    CHECK_THAT(outputs[2], WithinAbs(0.0f, 1e-6f)); // extra output zeroed
}

TEST_CASE("DirectWire: all-one inputs with identity-like weights", "[brain]") {
    DirectWireNetwork net(4, 4);
    // Diagonal weights
    for (int i = 0; i < 4; ++i) {
        net.set_weight(i, i, 2.0f);
    }

    float inputs[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float outputs[4] = {};
    net.activate(inputs, 4, outputs, 4);

    for (int i = 0; i < 4; ++i) {
        CHECK_THAT(outputs[i], WithinAbs(sigmoid(2.0f), 1e-5f));
    }
}

TEST_CASE("DirectWire: polymorphic use through ProcessingNetwork pointer", "[brain]") {
    auto net = std::make_unique<DirectWireNetwork>(2, 2);
    net->set_weight(0, 1, 3.0f);

    ProcessingNetwork* brain = net.get();

    float inputs[] = {1.0f, 0.0f};
    float outputs[2] = {};
    brain->activate(inputs, 2, outputs, 2);

    CHECK_THAT(outputs[0], WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(outputs[1], WithinAbs(sigmoid(3.0f), 1e-5f));
}
