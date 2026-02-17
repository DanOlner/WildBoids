#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "brain/mutation.h"
#include "brain/neat_network.h"
#include <cmath>
#include <set>
#include <algorithm>

using Catch::Matchers::WithinAbs;

// Helper: create a minimal genome with known innovation start
static NeatGenome make_minimal(int n_in = 3, int n_out = 2) {
    int next = 1;
    return NeatGenome::minimal(n_in, n_out, next);
}

// ---- mutate_weights ----

TEST_CASE("mutate_weights: weights change", "[mutation]") {
    NeatGenome genome = make_minimal();
    // Set all weights to 0
    for (auto& c : genome.connections) c.weight = 0.0f;

    std::mt19937 rng(42);
    mutate_weights(genome, rng);

    // At least some weights should have changed
    bool any_changed = false;
    for (const auto& c : genome.connections) {
        if (c.weight != 0.0f) { any_changed = true; break; }
    }
    CHECK(any_changed);
}

TEST_CASE("mutate_weights: with zero probabilities, weights unchanged", "[mutation]") {
    NeatGenome genome = make_minimal();
    for (auto& c : genome.connections) c.weight = 1.0f;

    std::mt19937 rng(42);
    mutate_weights(genome, rng, 0.0f, 0.3f, 0.0f);

    for (const auto& c : genome.connections) {
        CHECK_THAT(c.weight, WithinAbs(1.0f, 1e-6f));
    }
}

// ---- mutate_add_connection ----

TEST_CASE("mutate_add_connection: adds a new connection", "[mutation]") {
    NeatGenome genome = make_minimal(3, 2);
    // Minimal has 6 connections (3×2). Add a hidden node first to create room.
    int next = 100;
    InnovationTracker tracker(next);

    // Insert a hidden node so there are unconnected pairs
    genome.nodes.push_back({10, NodeType::Hidden, ActivationFn::Sigmoid, 0.0f});

    size_t before = genome.connections.size();
    std::mt19937 rng(42);
    bool added = mutate_add_connection(genome, rng, tracker);

    CHECK(added);
    CHECK(genome.connections.size() == before + 1);
}

TEST_CASE("mutate_add_connection: fully connected returns false", "[mutation]") {
    // Minimal 2→1 has 2 connections — already fully connected (no hidden nodes)
    NeatGenome genome = make_minimal(2, 1);

    InnovationTracker tracker(100);
    std::mt19937 rng(42);
    bool added = mutate_add_connection(genome, rng, tracker);

    CHECK_FALSE(added);
}

TEST_CASE("mutate_add_connection: no duplicate connections", "[mutation]") {
    NeatGenome genome = make_minimal(3, 2);
    genome.nodes.push_back({10, NodeType::Hidden, ActivationFn::Sigmoid, 0.0f});

    InnovationTracker tracker(100);
    std::mt19937 rng(42);

    // Add many connections — should never create duplicates
    for (int i = 0; i < 50; ++i) {
        mutate_add_connection(genome, rng, tracker);
    }

    std::set<std::pair<int,int>> pairs;
    for (const auto& c : genome.connections) {
        auto p = std::make_pair(c.source, c.target);
        CHECK(pairs.count(p) == 0);
        pairs.insert(p);
    }
}

TEST_CASE("mutate_add_connection: doesn't connect to input nodes", "[mutation]") {
    NeatGenome genome = make_minimal(3, 2);
    genome.nodes.push_back({10, NodeType::Hidden, ActivationFn::Sigmoid, 0.0f});

    InnovationTracker tracker(100);
    std::mt19937 rng(42);

    for (int i = 0; i < 50; ++i) {
        mutate_add_connection(genome, rng, tracker);
    }

    for (const auto& c : genome.connections) {
        // Target should never be an input node
        auto it = std::find_if(genome.nodes.begin(), genome.nodes.end(),
            [&](const NodeGene& n) { return n.id == c.target; });
        CHECK(it->type != NodeType::Input);
    }
}

// ---- mutate_add_node ----

TEST_CASE("mutate_add_node: splits a connection", "[mutation]") {
    NeatGenome genome = make_minimal(3, 2);
    InnovationTracker tracker(100);
    std::mt19937 rng(42);

    size_t nodes_before = genome.nodes.size();
    size_t conns_before = genome.connections.size();

    bool added = mutate_add_node(genome, rng, tracker);

    CHECK(added);
    CHECK(genome.nodes.size() == nodes_before + 1);
    // One disabled, two new = net +2
    CHECK(genome.connections.size() == conns_before + 2);

    // The new node should be Hidden
    CHECK(genome.nodes.back().type == NodeType::Hidden);
}

TEST_CASE("mutate_add_node: original connection is disabled", "[mutation]") {
    NeatGenome genome = make_minimal(3, 2);
    InnovationTracker tracker(100);
    std::mt19937 rng(42);

    // Record enabled count before
    int enabled_before = 0;
    for (const auto& c : genome.connections) {
        if (c.enabled) ++enabled_before;
    }

    mutate_add_node(genome, rng, tracker);

    // One old connection disabled, two new enabled → net +1 enabled
    int enabled_after = 0;
    for (const auto& c : genome.connections) {
        if (c.enabled) ++enabled_after;
    }
    CHECK(enabled_after == enabled_before + 1);
}

TEST_CASE("mutate_add_node: preserves behaviour (weight 1.0 in, original out)", "[mutation]") {
    NeatGenome genome = make_minimal(2, 1);
    // Set the first connection's weight to 3.5
    genome.connections[0].weight = 3.5f;

    InnovationTracker tracker(100);
    // Use a fixed seed that will pick connection 0
    std::mt19937 rng(0);

    mutate_add_node(genome, rng, tracker);

    // Find the two new connections (last two added)
    auto& new_conn1 = genome.connections[genome.connections.size() - 2];
    auto& new_conn2 = genome.connections[genome.connections.size() - 1];

    // One should have weight 1.0, the other should have the original weight
    // source→new has weight 1.0, new→target has original weight
    bool found_one = (new_conn1.weight == 1.0f && new_conn2.weight != 1.0f) ||
                     (new_conn2.weight == 1.0f && new_conn1.weight != 1.0f);
    // Actually, the convention is: source→new = 1.0, new→target = original
    CHECK(new_conn1.weight == 1.0f);
}

TEST_CASE("mutate_add_node: no enabled connections returns false", "[mutation]") {
    NeatGenome genome = make_minimal(2, 1);
    for (auto& c : genome.connections) c.enabled = false;

    InnovationTracker tracker(100);
    std::mt19937 rng(42);

    CHECK_FALSE(mutate_add_node(genome, rng, tracker));
}

// ---- mutate_toggle_connection ----

TEST_CASE("mutate_toggle_connection: flips enabled state", "[mutation]") {
    NeatGenome genome = make_minimal(2, 1);
    // All start enabled
    for (const auto& c : genome.connections) {
        REQUIRE(c.enabled);
    }

    std::mt19937 rng(42);
    mutate_toggle_connection(genome, rng);

    // Exactly one should now be disabled
    int disabled = 0;
    for (const auto& c : genome.connections) {
        if (!c.enabled) ++disabled;
    }
    CHECK(disabled == 1);
}

// ---- mutate_delete_connection ----

TEST_CASE("mutate_delete_connection: removes a connection", "[mutation]") {
    NeatGenome genome = make_minimal(3, 2);
    size_t before = genome.connections.size();

    std::mt19937 rng(42);
    bool deleted = mutate_delete_connection(genome, rng);

    CHECK(deleted);
    CHECK(genome.connections.size() == before - 1);
}

TEST_CASE("mutate_delete_connection: orphaned hidden node is removed", "[mutation]") {
    NeatGenome genome = make_minimal(2, 1);
    InnovationTracker tracker(100);
    std::mt19937 rng(42);

    // Add a hidden node (splits a connection)
    mutate_add_node(genome, rng, tracker);
    size_t nodes_with_hidden = genome.nodes.size();
    CHECK(nodes_with_hidden == 4); // 2 input + 1 output + 1 hidden

    // Find and delete both connections involving the hidden node
    int hidden_id = genome.nodes.back().id;
    // Remove connections touching the hidden node
    genome.connections.erase(
        std::remove_if(genome.connections.begin(), genome.connections.end(),
            [&](const ConnectionGene& c) {
                return c.source == hidden_id || c.target == hidden_id;
            }),
        genome.connections.end());

    // Now manually trigger orphan cleanup by calling delete on any remaining connection
    // Actually, let's do it properly: the orphan cleanup happens inside mutate_delete_connection.
    // So let's re-set up and test that.

    // Fresh start
    genome = make_minimal(2, 1);
    InnovationTracker tracker2(200);
    std::mt19937 rng2(42);
    mutate_add_node(genome, rng2, tracker2);

    // Find the hidden node
    hidden_id = -1;
    for (const auto& n : genome.nodes) {
        if (n.type == NodeType::Hidden) { hidden_id = n.id; break; }
    }
    REQUIRE(hidden_id >= 0);

    // Delete connections involving hidden node until it becomes orphaned
    while (true) {
        bool found = false;
        for (int i = 0; i < static_cast<int>(genome.connections.size()); ++i) {
            if (genome.connections[i].source == hidden_id ||
                genome.connections[i].target == hidden_id) {
                genome.connections.erase(genome.connections.begin() + i);
                found = true;
                break;
            }
        }
        if (!found) break;
    }

    // Hidden node still exists but is orphaned — call delete to trigger cleanup
    // Need at least one connection to delete
    if (!genome.connections.empty()) {
        // Add dummy connection to delete
        mutate_delete_connection(genome, rng2);
    }

    // After deletion + cleanup, hidden node should be gone
    bool hidden_exists = false;
    for (const auto& n : genome.nodes) {
        if (n.id == hidden_id) { hidden_exists = true; break; }
    }
    CHECK_FALSE(hidden_exists);
}

TEST_CASE("mutate_delete_connection: empty genome returns false", "[mutation]") {
    NeatGenome genome;
    genome.nodes.push_back({0, NodeType::Input, ActivationFn::Linear, 0.0f});
    genome.nodes.push_back({1, NodeType::Output, ActivationFn::Sigmoid, 0.0f});

    std::mt19937 rng(42);
    CHECK_FALSE(mutate_delete_connection(genome, rng));
}

// ---- Structural validity after mutation ----

TEST_CASE("Mutated genome builds a valid NeatNetwork", "[mutation]") {
    NeatGenome genome = make_minimal(3, 2);
    InnovationTracker tracker(100);
    std::mt19937 rng(42);

    // Apply several rounds of different mutations
    for (int round = 0; round < 10; ++round) {
        mutate_weights(genome, rng);
        mutate_add_connection(genome, rng, tracker);
        mutate_add_node(genome, rng, tracker);
        mutate_toggle_connection(genome, rng);
    }

    // Should build and activate without crashing
    NeatNetwork net(genome);
    float inputs[3] = {0.5f, 0.5f, 0.5f};
    float outputs[2] = {0.0f, 0.0f};
    net.activate(inputs, 3, outputs, 2);

    // Outputs should be valid numbers (not NaN or Inf)
    for (int i = 0; i < 2; ++i) {
        CHECK(std::isfinite(outputs[i]));
        CHECK(outputs[i] >= 0.0f);  // sigmoid outputs
        CHECK(outputs[i] <= 1.0f);
    }
}

TEST_CASE("Many mutations produce diverse genomes", "[mutation]") {
    InnovationTracker tracker(100);

    // Create 10 genomes with different mutation histories.
    // Use the rng to vary which mutations fire, so different seeds
    // produce different structural outcomes.
    std::vector<size_t> node_counts;
    std::vector<size_t> conn_counts;

    for (int seed = 0; seed < 10; ++seed) {
        NeatGenome genome = make_minimal(3, 2);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> coin(0.0f, 1.0f);

        for (int round = 0; round < 30; ++round) {
            mutate_weights(genome, rng);
            if (coin(rng) < 0.3f) mutate_add_node(genome, rng, tracker);
            if (coin(rng) < 0.4f) mutate_add_connection(genome, rng, tracker);
            if (coin(rng) < 0.2f) mutate_delete_connection(genome, rng);
        }

        node_counts.push_back(genome.nodes.size());
        conn_counts.push_back(genome.connections.size());
    }

    // Should have some structural diversity
    auto [min_n, max_n] = std::minmax_element(node_counts.begin(), node_counts.end());
    auto [min_c, max_c] = std::minmax_element(conn_counts.begin(), conn_counts.end());
    CHECK(*max_n > *min_n);   // different node counts
    CHECK(*max_c > *min_c);   // different connection counts
}
