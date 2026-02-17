#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "brain/speciation.h"
#include "brain/mutation.h"
#include "brain/innovation_tracker.h"

using Catch::Matchers::WithinAbs;

static NeatGenome make_minimal(int n_in = 3, int n_out = 2) {
    int next = 1;
    return NeatGenome::minimal(n_in, n_out, next);
}

// ---- compatibility_distance tests ----

TEST_CASE("Compatibility: identical genomes have distance 0", "[speciation]") {
    NeatGenome g = make_minimal();
    CompatibilityParams params;
    CHECK_THAT(compatibility_distance(g, g, params), WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Compatibility: weight difference only", "[speciation]") {
    NeatGenome a = make_minimal();
    NeatGenome b = make_minimal();

    // Set all weights in a to 1.0, in b to 0.0
    for (auto& c : a.connections) c.weight = 1.0f;
    for (auto& c : b.connections) c.weight = 0.0f;

    CompatibilityParams params;
    params.c1 = 0; params.c2 = 0; params.c3 = 1.0f;
    float dist = compatibility_distance(a, b, params);

    // Mean weight diff = 1.0, c3 = 1.0 → distance = 1.0
    CHECK_THAT(dist, WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Compatibility: excess genes increase distance", "[speciation]") {
    NeatGenome a = make_minimal(2, 1);
    NeatGenome b = make_minimal(2, 1);

    // Add excess gene to a (innovation beyond b's max)
    a.connections.push_back({100, 0, 2, 0.0f, true});

    CompatibilityParams params;
    params.c1 = 1.0f; params.c2 = 0; params.c3 = 0;
    params.normalise_threshold = 1;  // N = max(genome_size, 1) = 3

    float dist = compatibility_distance(a, b, params);
    // 1 excess gene, N = 3 → c1 * 1 / 3 = 0.333
    CHECK(dist > 0.0f);
}

TEST_CASE("Compatibility: disjoint genes increase distance", "[speciation]") {
    NeatGenome a = make_minimal(2, 1);
    NeatGenome b = make_minimal(2, 1);

    // Give b a high innovation gene so a's middle innovations become disjoint
    // a has innovations [1, 2], add innovation 50 to both, then add innovation 3 only to a
    a.connections.push_back({3, 0, 2, 0.0f, true});
    a.connections.push_back({50, 1, 2, 0.0f, true});
    b.connections.push_back({50, 1, 2, 0.0f, true});

    CompatibilityParams params;
    params.c1 = 0; params.c2 = 1.0f; params.c3 = 0;
    params.normalise_threshold = 1;

    float dist = compatibility_distance(a, b, params);
    // Innovation 3 is in a but not b, and 3 < min(50, 50) = 50, so it's disjoint
    CHECK(dist > 0.0f);
}

TEST_CASE("Compatibility: empty genomes have distance 0", "[speciation]") {
    NeatGenome a, b;
    CompatibilityParams params;
    CHECK_THAT(compatibility_distance(a, b, params), WithinAbs(0.0f, 1e-6f));
}

// ---- assign_species tests ----

TEST_CASE("assign_species: all identical go to one species", "[speciation]") {
    std::vector<NeatGenome> genomes;
    for (int i = 0; i < 10; ++i) {
        genomes.push_back(make_minimal());
    }

    std::vector<Species> species;
    CompatibilityParams params;
    int next_id = 1;
    assign_species(species, genomes, params, 3.0f, next_id);

    CHECK(species.size() == 1);
    CHECK(species[0].members.size() == 10);
}

TEST_CASE("assign_species: divergent genomes create multiple species", "[speciation]") {
    std::vector<NeatGenome> genomes;
    InnovationTracker tracker(100);

    // Create two groups of genomes with very different structures
    for (int i = 0; i < 5; ++i) {
        genomes.push_back(make_minimal());
    }
    for (int i = 0; i < 5; ++i) {
        NeatGenome g = make_minimal();
        std::mt19937 rng(i + 100);
        // Heavy mutation to make them very different
        for (int j = 0; j < 10; ++j) {
            mutate_add_node(g, rng, tracker);
            mutate_add_connection(g, rng, tracker);
            mutate_weights(g, rng);
        }
        genomes.push_back(std::move(g));
    }

    std::vector<Species> species;
    CompatibilityParams params;
    int next_id = 1;
    assign_species(species, genomes, params, 1.0f, next_id);  // tight threshold

    CHECK(species.size() > 1);

    // All genomes should be assigned
    int total = 0;
    for (const auto& s : species) total += static_cast<int>(s.members.size());
    CHECK(total == 10);
}

TEST_CASE("assign_species: empty species are removed", "[speciation]") {
    std::vector<NeatGenome> genomes;
    genomes.push_back(make_minimal());

    // Pre-populate with a species whose representative is very different
    std::vector<Species> species;
    Species old;
    old.id = 99;
    NeatGenome weird = make_minimal();
    InnovationTracker tracker(100);
    std::mt19937 rng(42);
    for (int i = 0; i < 20; ++i) {
        mutate_add_node(weird, rng, tracker);
        mutate_weights(weird, rng);
    }
    old.representative = weird;
    species.push_back(std::move(old));

    CompatibilityParams params;
    int next_id = 100;
    assign_species(species, genomes, params, 0.5f, next_id);  // tight threshold

    // The old empty species should be gone, genome placed in a new species
    for (const auto& s : species) {
        CHECK(!s.members.empty());
    }
    int total = 0;
    for (const auto& s : species) total += static_cast<int>(s.members.size());
    CHECK(total == 1);
}
