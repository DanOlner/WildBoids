#include <catch2/catch_test_macros.hpp>
#include "brain/neat_genome.h"
#include <set>

TEST_CASE("Minimal genome: correct node count and types", "[neat_genome]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(7, 4, next_innov);

    REQUIRE(g.nodes.size() == 11); // 7 input + 4 output

    int input_count = 0, output_count = 0, hidden_count = 0;
    for (const auto& n : g.nodes) {
        switch (n.type) {
            case NodeType::Input:  ++input_count;  break;
            case NodeType::Output: ++output_count; break;
            case NodeType::Hidden: ++hidden_count; break;
        }
    }

    CHECK(input_count == 7);
    CHECK(output_count == 4);
    CHECK(hidden_count == 0);
}

TEST_CASE("Minimal genome: fully connected input→output", "[neat_genome]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(7, 4, next_innov);

    // 7 inputs × 4 outputs = 28 connections
    REQUIRE(g.connections.size() == 28);

    // All connections should be enabled
    for (const auto& c : g.connections) {
        CHECK(c.enabled);
    }
}

TEST_CASE("Minimal genome: unique innovation numbers", "[neat_genome]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(7, 4, next_innov);

    std::set<int> innovations;
    for (const auto& c : g.connections) {
        innovations.insert(c.innovation);
    }

    // All 28 innovations should be unique
    CHECK(innovations.size() == 28);
    // next_innovation should have advanced by 28
    CHECK(next_innov == 29);
}

TEST_CASE("Minimal genome: connections link inputs to outputs only", "[neat_genome]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(3, 2, next_innov);

    // Input ids: 0, 1, 2. Output ids: 3, 4.
    for (const auto& c : g.connections) {
        // Source must be an input node (id < 3)
        CHECK(c.source < 3);
        // Target must be an output node (id >= 3)
        CHECK(c.target >= 3);
        CHECK(c.target < 5);
    }
}

TEST_CASE("Minimal genome: node ids are sequential", "[neat_genome]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(3, 2, next_innov);

    REQUIRE(g.nodes.size() == 5);
    for (int i = 0; i < 5; ++i) {
        CHECK(g.nodes[i].id == i);
    }
}

TEST_CASE("Minimal genome: input nodes have Linear activation", "[neat_genome]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(3, 2, next_innov);

    for (const auto& n : g.nodes) {
        if (n.type == NodeType::Input) {
            CHECK(n.activation == ActivationFn::Linear);
        }
    }
}

TEST_CASE("Minimal genome: output nodes have Sigmoid activation", "[neat_genome]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(3, 2, next_innov);

    for (const auto& n : g.nodes) {
        if (n.type == NodeType::Output) {
            CHECK(n.activation == ActivationFn::Sigmoid);
        }
    }
}

TEST_CASE("Minimal genome: all weights start at zero", "[neat_genome]") {
    int next_innov = 1;
    NeatGenome g = NeatGenome::minimal(7, 4, next_innov);

    for (const auto& c : g.connections) {
        CHECK(c.weight == 0.0f);
    }
}

TEST_CASE("Minimal genome: innovation counter is consumed correctly", "[neat_genome]") {
    int next_innov = 100;
    NeatGenome g = NeatGenome::minimal(2, 3, next_innov);

    // 2×3 = 6 connections, starting at innovation 100
    CHECK(g.connections[0].innovation == 100);
    CHECK(g.connections[5].innovation == 105);
    CHECK(next_innov == 106);
}
