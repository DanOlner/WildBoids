#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "brain/crossover.h"
#include "brain/mutation.h"
#include "brain/innovation_tracker.h"
#include "brain/neat_network.h"
#include <cmath>
#include <set>
#include <algorithm>
#include <unordered_map>

using Catch::Matchers::WithinAbs;

static NeatGenome make_minimal(int n_in = 3, int n_out = 2) {
    int next = 1;
    return NeatGenome::minimal(n_in, n_out, next);
}

TEST_CASE("Crossover: identical parents produce identical offspring", "[crossover]") {
    NeatGenome parent = make_minimal();
    // Give connections some weights
    for (int i = 0; i < static_cast<int>(parent.connections.size()); ++i) {
        parent.connections[i].weight = static_cast<float>(i) * 0.1f;
    }

    std::mt19937 rng(42);
    NeatGenome child = crossover(parent, parent, rng);

    CHECK(child.nodes.size() == parent.nodes.size());
    CHECK(child.connections.size() == parent.connections.size());

    // All weights should match one of the parents (which are identical)
    for (size_t i = 0; i < child.connections.size(); ++i) {
        CHECK_THAT(child.connections[i].weight,
                   WithinAbs(parent.connections[i].weight, 1e-6f));
    }
}

TEST_CASE("Crossover: fitter parent's excess genes inherited", "[crossover]") {
    NeatGenome fitter = make_minimal(2, 1);
    NeatGenome other = make_minimal(2, 1);

    // Add an extra connection to the fitter parent (excess gene)
    fitter.nodes.push_back({10, NodeType::Hidden, ActivationFn::Sigmoid, 0.0f});
    fitter.connections.push_back({100, 0, 10, 1.5f, true});
    fitter.connections.push_back({101, 10, 2, 2.0f, true});

    std::mt19937 rng(42);
    NeatGenome child = crossover(fitter, other, rng);

    // Child should have the excess connections (innovation 100, 101)
    bool has_100 = false, has_101 = false;
    for (const auto& c : child.connections) {
        if (c.innovation == 100) has_100 = true;
        if (c.innovation == 101) has_101 = true;
    }
    CHECK(has_100);
    CHECK(has_101);

    // Child should have the hidden node
    bool has_hidden = false;
    for (const auto& n : child.nodes) {
        if (n.id == 10) has_hidden = true;
    }
    CHECK(has_hidden);
}

TEST_CASE("Crossover: other parent's excess genes NOT inherited", "[crossover]") {
    NeatGenome fitter = make_minimal(2, 1);
    NeatGenome other = make_minimal(2, 1);

    // Add excess genes only to the other (less fit) parent
    other.nodes.push_back({10, NodeType::Hidden, ActivationFn::Sigmoid, 0.0f});
    other.connections.push_back({100, 0, 10, 1.5f, true});

    std::mt19937 rng(42);
    NeatGenome child = crossover(fitter, other, rng);

    // Child should NOT have innovation 100
    for (const auto& c : child.connections) {
        CHECK(c.innovation != 100);
    }
}

TEST_CASE("Crossover: matching genes from roughly 50/50 parents", "[crossover]") {
    NeatGenome parent_a = make_minimal(2, 1);
    NeatGenome parent_b = make_minimal(2, 1);

    // Set distinct weights so we can tell which parent a gene came from
    for (auto& c : parent_a.connections) c.weight = 1.0f;
    for (auto& c : parent_b.connections) c.weight = -1.0f;

    // Run crossover many times and count how often we get each parent's weight
    int from_a = 0, from_b = 0;
    for (int trial = 0; trial < 1000; ++trial) {
        std::mt19937 rng(trial);
        NeatGenome child = crossover(parent_a, parent_b, rng);
        for (const auto& c : child.connections) {
            if (c.weight == 1.0f) ++from_a;
            else if (c.weight == -1.0f) ++from_b;
        }
    }

    // Should be roughly 50/50 (allow ±10%)
    float ratio = static_cast<float>(from_a) / static_cast<float>(from_a + from_b);
    CHECK(ratio > 0.4f);
    CHECK(ratio < 0.6f);
}

TEST_CASE("Crossover: disabled gene handling", "[crossover]") {
    NeatGenome parent_a = make_minimal(2, 1);
    NeatGenome parent_b = make_minimal(2, 1);

    // Disable connection 1 in parent_a
    parent_a.connections[0].enabled = false;

    // Run many trials — should be disabled ~75% of the time
    int disabled_count = 0;
    int total = 1000;
    for (int trial = 0; trial < total; ++trial) {
        std::mt19937 rng(trial);
        NeatGenome child = crossover(parent_a, parent_b, rng);
        if (!child.connections[0].enabled) ++disabled_count;
    }

    float rate = static_cast<float>(disabled_count) / static_cast<float>(total);
    // Expected ~0.75, allow some variance
    CHECK(rate > 0.6f);
    CHECK(rate < 0.9f);
}

TEST_CASE("Crossover: different hidden nodes produce valid offspring", "[crossover]") {
    NeatGenome parent_a = make_minimal(3, 2);
    NeatGenome parent_b = make_minimal(3, 2);

    InnovationTracker tracker(100);

    // Mutate each parent differently to get different hidden nodes
    std::mt19937 rng_a(42);
    std::mt19937 rng_b(99);
    for (int i = 0; i < 5; ++i) {
        mutate_add_node(parent_a, rng_a, tracker);
        mutate_add_node(parent_b, rng_b, tracker);
        mutate_add_connection(parent_a, rng_a, tracker);
    }

    std::mt19937 rng(0);
    NeatGenome child = crossover(parent_a, parent_b, rng);

    // Should have all input + output nodes
    int inputs = 0, outputs = 0;
    for (const auto& n : child.nodes) {
        if (n.type == NodeType::Input) ++inputs;
        if (n.type == NodeType::Output) ++outputs;
    }
    CHECK(inputs == 3);
    CHECK(outputs == 2);

    // No duplicate node IDs
    std::set<int> ids;
    for (const auto& n : child.nodes) {
        CHECK(ids.count(n.id) == 0);
        ids.insert(n.id);
    }

    // No duplicate innovation numbers
    std::set<int> innovs;
    for (const auto& c : child.connections) {
        CHECK(innovs.count(c.innovation) == 0);
        innovs.insert(c.innovation);
    }
}

TEST_CASE("Crossover: offspring builds a valid NeatNetwork", "[crossover]") {
    NeatGenome parent_a = make_minimal(3, 2);
    NeatGenome parent_b = make_minimal(3, 2);

    InnovationTracker tracker(100);
    std::mt19937 rng_a(42);
    std::mt19937 rng_b(99);

    // Add structural complexity
    for (int i = 0; i < 5; ++i) {
        mutate_add_node(parent_a, rng_a, tracker);
        mutate_weights(parent_a, rng_a);
        mutate_add_node(parent_b, rng_b, tracker);
        mutate_weights(parent_b, rng_b);
    }

    std::mt19937 rng(0);
    NeatGenome child = crossover(parent_a, parent_b, rng);

    // Should build and activate without crashing
    NeatNetwork net(child);
    float inputs[3] = {0.5f, 0.5f, 0.5f};
    float outputs[2] = {0.0f, 0.0f};
    net.activate(inputs, 3, outputs, 2);

    for (int i = 0; i < 2; ++i) {
        CHECK(std::isfinite(outputs[i]));
        CHECK(outputs[i] >= 0.0f);
        CHECK(outputs[i] <= 1.0f);
    }
}

TEST_CASE("Crossover: nodes sorted by id", "[crossover]") {
    NeatGenome parent = make_minimal(3, 2);
    InnovationTracker tracker(100);
    std::mt19937 rng_m(42);
    for (int i = 0; i < 3; ++i) {
        mutate_add_node(parent, rng_m, tracker);
    }

    std::mt19937 rng(0);
    NeatGenome child = crossover(parent, parent, rng);

    for (size_t i = 1; i < child.nodes.size(); ++i) {
        CHECK(child.nodes[i].id > child.nodes[i - 1].id);
    }
}
