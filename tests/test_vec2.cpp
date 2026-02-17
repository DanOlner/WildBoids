#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/vec2.h"
#include <numbers>

using Catch::Matchers::WithinAbs;

TEST_CASE("Vec2 arithmetic", "[vec2]") {
    Vec2 a{3, 4};
    Vec2 b{1, 2};

    SECTION("addition") {
        Vec2 c = a + b;
        CHECK(c.x == 4);
        CHECK(c.y == 6);
    }

    SECTION("subtraction") {
        Vec2 c = a - b;
        CHECK(c.x == 2);
        CHECK(c.y == 2);
    }

    SECTION("scalar multiply") {
        Vec2 c = a * 2;
        CHECK(c.x == 6);
        CHECK(c.y == 8);

        Vec2 d = 2 * a;
        CHECK(d.x == 6);
        CHECK(d.y == 8);
    }

    SECTION("scalar divide") {
        Vec2 c = a / 2;
        CHECK_THAT(c.x, WithinAbs(1.5f, 1e-6f));
        CHECK_THAT(c.y, WithinAbs(2.0f, 1e-6f));
    }

    SECTION("compound assignment") {
        Vec2 c = a;
        c += b;
        CHECK(c.x == 4);
        c -= b;
        CHECK(c.x == 3);
        c *= 2;
        CHECK(c.x == 6);
    }
}

TEST_CASE("Vec2 length and dot", "[vec2]") {
    Vec2 a{3, 4};
    CHECK_THAT(a.length(), WithinAbs(5.0f, 1e-6f));
    CHECK_THAT(a.length_squared(), WithinAbs(25.0f, 1e-6f));

    Vec2 b{1, 0};
    Vec2 c{0, 1};
    CHECK_THAT(b.dot(c), WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(b.dot(b), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Vec2 normalized", "[vec2]") {
    Vec2 a{3, 4};
    Vec2 n = a.normalized();
    CHECK_THAT(n.length(), WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(n.x, WithinAbs(0.6f, 1e-5f));
    CHECK_THAT(n.y, WithinAbs(0.8f, 1e-5f));

    // Zero vector normalises to zero
    Vec2 zero{0, 0};
    Vec2 zn = zero.normalized();
    CHECK(zn.x == 0);
    CHECK(zn.y == 0);
}

TEST_CASE("Vec2 rotation", "[vec2]") {
    Vec2 right{1, 0};

    SECTION("90 degrees CCW gives (0, 1)") {
        Vec2 up = right.rotated(std::numbers::pi_v<float> / 2);
        CHECK_THAT(up.x, WithinAbs(0.0f, 1e-5f));
        CHECK_THAT(up.y, WithinAbs(1.0f, 1e-5f));
    }

    SECTION("180 degrees gives (-1, 0)") {
        Vec2 left = right.rotated(std::numbers::pi_v<float>);
        CHECK_THAT(left.x, WithinAbs(-1.0f, 1e-5f));
        CHECK_THAT(left.y, WithinAbs(0.0f, 1e-5f));
    }

    SECTION("full rotation returns to start") {
        Vec2 full = right.rotated(2 * std::numbers::pi_v<float>);
        CHECK_THAT(full.x, WithinAbs(1.0f, 1e-5f));
        CHECK_THAT(full.y, WithinAbs(0.0f, 1e-5f));
    }
}

TEST_CASE("cross2d", "[vec2]") {
    Vec2 right{1, 0};
    Vec2 up{0, 1};

    // right × up = +1 (CCW)
    CHECK_THAT(cross2d(right, up), WithinAbs(1.0f, 1e-6f));
    // up × right = -1 (CW)
    CHECK_THAT(cross2d(up, right), WithinAbs(-1.0f, 1e-6f));
    // parallel vectors have zero cross product
    CHECK_THAT(cross2d(right, right), WithinAbs(0.0f, 1e-6f));
}
