#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/toroidal.h"

using Catch::Matchers::WithinAbs;

static constexpr float W = 800.0f;
static constexpr float H = 800.0f;

TEST_CASE("Toroidal delta within half-world is raw difference", "[toroidal]") {
    Vec2 a{100, 200};
    Vec2 b{300, 350};
    Vec2 d = toroidal_delta(a, b, W, H);
    REQUIRE_THAT(d.x, WithinAbs(200.0f, 0.01f));
    REQUIRE_THAT(d.y, WithinAbs(150.0f, 0.01f));
}

TEST_CASE("Toroidal delta wraps across X boundary", "[toroidal]") {
    // a at x=750, b at x=50 — raw diff = -700, wrapped = +100 (short path)
    Vec2 a{750, 400};
    Vec2 b{50, 400};
    Vec2 d = toroidal_delta(a, b, W, H);
    REQUIRE_THAT(d.x, WithinAbs(100.0f, 0.01f));
    REQUIRE_THAT(d.y, WithinAbs(0.0f, 0.01f));

    // Reverse direction
    Vec2 d2 = toroidal_delta(b, a, W, H);
    REQUIRE_THAT(d2.x, WithinAbs(-100.0f, 0.01f));
}

TEST_CASE("Toroidal delta wraps across Y boundary", "[toroidal]") {
    Vec2 a{400, 780};
    Vec2 b{400, 20};
    Vec2 d = toroidal_delta(a, b, W, H);
    REQUIRE_THAT(d.x, WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(d.y, WithinAbs(40.0f, 0.01f));
}

TEST_CASE("Toroidal delta wraps across both axes (corner)", "[toroidal]") {
    Vec2 a{790, 790};
    Vec2 b{10, 10};
    Vec2 d = toroidal_delta(a, b, W, H);
    REQUIRE_THAT(d.x, WithinAbs(20.0f, 0.01f));
    REQUIRE_THAT(d.y, WithinAbs(20.0f, 0.01f));
}

TEST_CASE("toroidal_distance_sq matches delta length_squared", "[toroidal]") {
    Vec2 a{750, 780};
    Vec2 b{30, 20};
    Vec2 d = toroidal_delta(a, b, W, H);
    float dist_sq = toroidal_distance_sq(a, b, W, H);
    REQUIRE_THAT(dist_sq, WithinAbs(d.length_squared(), 0.01f));
}
