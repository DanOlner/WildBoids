#include <catch2/catch_test_macros.hpp>
#include "simulation/spatial_grid.h"
#include "simulation/toroidal.h"
#include "simulation/world.h"
#include <algorithm>
#include <random>
#include <set>

static constexpr float W = 800.0f;
static constexpr float H = 800.0f;
static constexpr float CELL = 100.0f;

TEST_CASE("Grid insert and query finds nearby boid", "[spatial_grid]") {
    SpatialGrid grid(W, H, CELL, true);
    grid.insert(0, {150, 150});
    grid.insert(1, {160, 160});
    grid.insert(2, {600, 600});

    std::vector<int> results;
    grid.query({155, 155}, CELL, results);

    // Should find boids 0 and 1 (same cell area), not boid 2
    REQUIRE(std::find(results.begin(), results.end(), 0) != results.end());
    REQUIRE(std::find(results.begin(), results.end(), 1) != results.end());
    REQUIRE(std::find(results.begin(), results.end(), 2) == results.end());
}

TEST_CASE("Grid query near X edge finds wrapped boids (toroidal)", "[spatial_grid]") {
    SpatialGrid grid(W, H, CELL, true);
    // Boid near left edge
    grid.insert(0, {10, 400});
    // Boid near right edge
    grid.insert(1, {790, 400});

    std::vector<int> results;
    // Query from near the right edge — should find boid on left edge via wrapping
    grid.query({790, 400}, CELL, results);
    REQUIRE(std::find(results.begin(), results.end(), 0) != results.end());
    REQUIRE(std::find(results.begin(), results.end(), 1) != results.end());
}

TEST_CASE("Grid query near Y edge finds wrapped boids (toroidal)", "[spatial_grid]") {
    SpatialGrid grid(W, H, CELL, true);
    grid.insert(0, {400, 5});
    grid.insert(1, {400, 795});

    std::vector<int> results;
    grid.query({400, 795}, CELL, results);
    REQUIRE(std::find(results.begin(), results.end(), 0) != results.end());
    REQUIRE(std::find(results.begin(), results.end(), 1) != results.end());
}

TEST_CASE("Grid query at corner finds boids across both axis wraps", "[spatial_grid]") {
    SpatialGrid grid(W, H, CELL, true);
    grid.insert(0, {790, 790});
    grid.insert(1, {10, 10});

    std::vector<int> results;
    grid.query({790, 790}, CELL, results);
    REQUIRE(std::find(results.begin(), results.end(), 0) != results.end());
    REQUIRE(std::find(results.begin(), results.end(), 1) != results.end());
}

TEST_CASE("Grid clear removes all entries", "[spatial_grid]") {
    SpatialGrid grid(W, H, CELL, true);
    grid.insert(0, {100, 100});

    std::vector<int> results;
    grid.query({100, 100}, CELL, results);
    REQUIRE(!results.empty());

    grid.clear();
    results.clear();
    grid.query({100, 100}, CELL, results);
    REQUIRE(results.empty());
}

TEST_CASE("Non-toroidal grid does not wrap at edges", "[spatial_grid]") {
    SpatialGrid grid(W, H, CELL, false);
    grid.insert(0, {10, 400});
    grid.insert(1, {790, 400});

    std::vector<int> results;
    // Query from right edge — should NOT find boid on left edge
    grid.query({790, 400}, CELL, results);
    REQUIRE(std::find(results.begin(), results.end(), 0) == results.end());
    REQUIRE(std::find(results.begin(), results.end(), 1) != results.end());
}

TEST_CASE("Grid query with small radius has no false negatives", "[spatial_grid]") {
    // Insert boids at known positions and verify that a query with radius
    // covering them returns all of them
    SpatialGrid grid(W, H, CELL, true);
    grid.insert(0, {200, 200});
    grid.insert(1, {230, 210});  // ~34 units away
    grid.insert(2, {500, 500});  // far away

    std::vector<int> results;
    grid.query({200, 200}, 50.0f, results);

    // Grid returns broad-phase candidates — both 0 and 1 should be there
    REQUIRE(std::find(results.begin(), results.end(), 0) != results.end());
    REQUIRE(std::find(results.begin(), results.end(), 1) != results.end());
}

TEST_CASE("World grid matches brute-force neighbour check", "[spatial_grid]") {
    WorldConfig config;
    config.width = W;
    config.height = H;
    config.toroidal = true;
    config.grid_cell_size = CELL;
    config.linear_drag = 0.02f;
    config.angular_drag = 0.05f;

    World world(config);

    // Spawn 50 boids at random positions
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> pos_x(0, W);
    std::uniform_real_distribution<float> pos_y(0, H);

    for (int i = 0; i < 50; ++i) {
        Boid boid;
        boid.type = "prey";
        boid.body.position = {pos_x(rng), pos_y(rng)};
        world.add_boid(std::move(boid));
    }

    // Step once to rebuild the grid
    world.step(1.0f / 120.0f);

    const auto& boids = world.get_boids();
    const auto& grid = world.grid();
    float query_radius = 150.0f;

    // For each boid, compare grid results vs brute-force
    for (int i = 0; i < static_cast<int>(boids.size()); ++i) {
        Vec2 pos = boids[i].body.position;

        // Brute-force: find all boids within query_radius (toroidal)
        std::set<int> brute_force;
        for (int j = 0; j < static_cast<int>(boids.size()); ++j) {
            float d2 = toroidal_distance_sq(pos, boids[j].body.position, W, H);
            if (d2 <= query_radius * query_radius) {
                brute_force.insert(j);
            }
        }

        // Grid query (broad phase — may include extra candidates)
        std::vector<int> grid_results;
        grid.query(pos, query_radius, grid_results);
        std::set<int> grid_set(grid_results.begin(), grid_results.end());

        // Every brute-force hit must appear in grid results (no false negatives)
        for (int idx : brute_force) {
            REQUIRE(grid_set.count(idx) > 0);
        }
    }
}

TEST_CASE("World grid is consistent after multiple steps", "[spatial_grid]") {
    WorldConfig config;
    config.width = W;
    config.height = H;
    config.toroidal = true;
    config.grid_cell_size = CELL;

    World world(config);

    Boid boid;
    boid.type = "prey";
    boid.body.position = {400, 400};
    boid.body.velocity = {50, 30};
    world.add_boid(std::move(boid));

    // Step several times
    for (int i = 0; i < 100; ++i) {
        world.step(1.0f / 120.0f);
    }

    // Grid should find the boid near its current position
    Vec2 pos = world.get_boids()[0].body.position;
    std::vector<int> results;
    world.grid().query(pos, CELL, results);
    REQUIRE(std::find(results.begin(), results.end(), 0) != results.end());
}
