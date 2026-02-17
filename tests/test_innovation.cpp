#include <catch2/catch_test_macros.hpp>
#include "brain/innovation_tracker.h"

TEST_CASE("InnovationTracker: same pair returns same number", "[innovation]") {
    InnovationTracker tracker(1);
    int a = tracker.get_or_create(0, 7);
    int b = tracker.get_or_create(0, 7);
    CHECK(a == b);
}

TEST_CASE("InnovationTracker: different pairs get different numbers", "[innovation]") {
    InnovationTracker tracker(1);
    int a = tracker.get_or_create(0, 7);
    int b = tracker.get_or_create(1, 7);
    CHECK(a != b);
}

TEST_CASE("InnovationTracker: direction matters", "[innovation]") {
    InnovationTracker tracker(1);
    int a = tracker.get_or_create(0, 7);
    int b = tracker.get_or_create(7, 0);
    CHECK(a != b);
}

TEST_CASE("InnovationTracker: sequential numbering", "[innovation]") {
    InnovationTracker tracker(100);
    int a = tracker.get_or_create(0, 7);
    int b = tracker.get_or_create(1, 7);
    CHECK(a == 100);
    CHECK(b == 101);
}

TEST_CASE("InnovationTracker: new_generation resets cache", "[innovation]") {
    InnovationTracker tracker(1);
    int gen1 = tracker.get_or_create(0, 7);

    tracker.new_generation();

    // Same pair in a new generation gets a new number
    int gen2 = tracker.get_or_create(0, 7);
    CHECK(gen1 != gen2);
    CHECK(gen2 == gen1 + 1);
}

TEST_CASE("InnovationTracker: next_innovation reflects state", "[innovation]") {
    InnovationTracker tracker(1);
    CHECK(tracker.next_innovation() == 1);

    tracker.get_or_create(0, 7);
    CHECK(tracker.next_innovation() == 2);

    // Cached lookup doesn't increment
    tracker.get_or_create(0, 7);
    CHECK(tracker.next_innovation() == 2);
}
