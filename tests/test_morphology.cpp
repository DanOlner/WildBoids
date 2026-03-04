#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation/morphology_genome.h"
#include "io/boid_spec.h"
#include <cmath>
#include <filesystem>

static constexpr float PI = static_cast<float>(M_PI);
static constexpr float DEG_TO_RAD = PI / 180.0f;

static MorphologyEvolutionConfig make_two_group_config() {
    MorphologyEvolutionConfig config;
    config.enabled = true;
    config.groups = {
        {16, 360.0f, 100.0f},  // short-range: 16 eyes, 360° budget
        {4, 100.0f, 300.0f}    // long-range: 4 eyes, 100° budget
    };
    return config;
}

// --- Default morphology ---

TEST_CASE("Default morphology has correct group count", "[morphology]") {
    auto config = make_two_group_config();
    auto genome = create_default_morphology(config);
    REQUIRE(genome.groups.size() == 2);
}

TEST_CASE("Default morphology has correct eye counts", "[morphology]") {
    auto config = make_two_group_config();
    auto genome = create_default_morphology(config);
    CHECK(genome.groups[0].angles.size() == 16);
    CHECK(genome.groups[0].arc_fracs.size() == 16);
    CHECK(genome.groups[1].angles.size() == 4);
    CHECK(genome.groups[1].arc_fracs.size() == 4);
}

TEST_CASE("Default morphology has equal arc fractions", "[morphology]") {
    auto config = make_two_group_config();
    auto genome = create_default_morphology(config);
    for (auto f : genome.groups[0].arc_fracs) {
        CHECK(f == 1.0f);
    }
    for (auto f : genome.groups[1].arc_fracs) {
        CHECK(f == 1.0f);
    }
}

TEST_CASE("Default morphology has evenly spaced angles", "[morphology]") {
    auto config = make_two_group_config();
    auto genome = create_default_morphology(config);

    // 4 long-range eyes should be ~90° apart
    float step = 2.0f * PI / 4.0f;
    for (int i = 0; i < 4; ++i) {
        float expected = std::fmod(i * step + PI, 2.0f * PI) - PI;
        CHECK_THAT(genome.groups[1].angles[i],
                   Catch::Matchers::WithinAbs(expected, 0.01f));
    }
}

// --- Phenotype extraction ---

TEST_CASE("apply_morphology produces correct arc widths from equal fracs", "[morphology]") {
    auto config = make_two_group_config();
    auto genome = create_default_morphology(config);

    CompoundEyeConfig base;
    base.eyes.resize(16);
    base.long_range_eyes.resize(4);
    base.channels = {SensorChannel::Food};

    auto result = apply_morphology(base, genome, config);

    // 16 eyes sharing 360° equally → 22.5° each
    float expected_short = 360.0f / 16.0f * DEG_TO_RAD;
    for (const auto& eye : result.eyes) {
        CHECK_THAT(eye.arc_width, Catch::Matchers::WithinAbs(expected_short, 0.01f));
    }

    // 4 eyes sharing 100° equally → 25° each
    float expected_long = 100.0f / 4.0f * DEG_TO_RAD;
    for (const auto& eye : result.long_range_eyes) {
        CHECK_THAT(eye.arc_width, Catch::Matchers::WithinAbs(expected_long, 0.01f));
    }
}

TEST_CASE("apply_morphology arc widths sum to budget", "[morphology]") {
    auto config = make_two_group_config();
    auto genome = create_default_morphology(config);

    // Manually set unequal fracs
    genome.groups[0].arc_fracs = std::vector<float>(16, 1.0f);
    genome.groups[0].arc_fracs[0] = 3.0f;  // first eye gets more

    CompoundEyeConfig base;
    base.eyes.resize(16);
    base.long_range_eyes.resize(4);
    base.channels = {SensorChannel::Food};

    auto result = apply_morphology(base, genome, config);

    float total_arc = 0.0f;
    for (const auto& eye : result.eyes) {
        total_arc += eye.arc_width;
    }
    CHECK_THAT(total_arc, Catch::Matchers::WithinAbs(360.0f * DEG_TO_RAD, 0.01f));
}

TEST_CASE("apply_morphology preserves channels and proprioceptive sensors", "[morphology]") {
    auto config = make_two_group_config();
    auto genome = create_default_morphology(config);

    CompoundEyeConfig base;
    base.eyes.resize(16);
    base.long_range_eyes.resize(4);
    base.channels = {SensorChannel::Food, SensorChannel::Same};
    base.has_speed_sensor = true;
    base.has_angular_velocity_sensor = true;

    auto result = apply_morphology(base, genome, config);

    CHECK(result.channels.size() == 2);
    CHECK(result.has_speed_sensor == true);
    CHECK(result.has_angular_velocity_sensor == true);
}

TEST_CASE("apply_morphology sets max_range from config", "[morphology]") {
    auto config = make_two_group_config();
    auto genome = create_default_morphology(config);

    CompoundEyeConfig base;
    base.eyes.resize(16);
    base.long_range_eyes.resize(4);
    base.channels = {SensorChannel::Food};

    auto result = apply_morphology(base, genome, config);

    for (const auto& eye : result.eyes) {
        CHECK(eye.max_range == 100.0f);
    }
    for (const auto& eye : result.long_range_eyes) {
        CHECK(eye.max_range == 300.0f);
    }
}

// --- Mutation ---

TEST_CASE("Mutation changes at least some values", "[morphology]") {
    auto config = make_two_group_config();
    config.mutation.angle_mutate_prob = 1.0f;
    config.mutation.arc_mutate_prob = 1.0f;
    config.mutation.replace_prob = 0.0f;

    auto genome = create_default_morphology(config);
    auto original = genome;

    std::mt19937 rng(42);
    mutate_morphology(genome, config, rng);

    bool any_angle_changed = false;
    bool any_frac_changed = false;
    for (size_t gi = 0; gi < genome.groups.size(); ++gi) {
        for (size_t i = 0; i < genome.groups[gi].angles.size(); ++i) {
            if (genome.groups[gi].angles[i] != original.groups[gi].angles[i])
                any_angle_changed = true;
            if (genome.groups[gi].arc_fracs[i] != original.groups[gi].arc_fracs[i])
                any_frac_changed = true;
        }
    }
    CHECK(any_angle_changed);
    CHECK(any_frac_changed);
}

TEST_CASE("Mutation respects min_arc_frac clamp", "[morphology]") {
    auto config = make_two_group_config();
    config.mutation.arc_mutate_prob = 1.0f;
    config.mutation.arc_frac_sigma = 100.0f;  // very large to force negative values
    config.mutation.min_arc_frac = 0.1f;

    auto genome = create_default_morphology(config);

    std::mt19937 rng(42);
    for (int trial = 0; trial < 100; ++trial) {
        mutate_morphology(genome, config, rng);
        for (const auto& group : genome.groups) {
            for (float f : group.arc_fracs) {
                CHECK(f >= config.mutation.min_arc_frac);
            }
        }
    }
}

TEST_CASE("Mutation with zero probability changes nothing", "[morphology]") {
    auto config = make_two_group_config();
    config.mutation.angle_mutate_prob = 0.0f;
    config.mutation.arc_mutate_prob = 0.0f;

    auto genome = create_default_morphology(config);
    auto original = genome;

    std::mt19937 rng(42);
    mutate_morphology(genome, config, rng);

    for (size_t gi = 0; gi < genome.groups.size(); ++gi) {
        for (size_t i = 0; i < genome.groups[gi].angles.size(); ++i) {
            CHECK(genome.groups[gi].angles[i] == original.groups[gi].angles[i]);
            CHECK(genome.groups[gi].arc_fracs[i] == original.groups[gi].arc_fracs[i]);
        }
    }
}

TEST_CASE("Angles stay within [-pi, pi] after mutation", "[morphology]") {
    auto config = make_two_group_config();
    config.mutation.angle_mutate_prob = 1.0f;
    config.mutation.angle_sigma_deg = 180.0f;  // very large perturbations

    auto genome = create_default_morphology(config);
    std::mt19937 rng(42);

    for (int trial = 0; trial < 100; ++trial) {
        mutate_morphology(genome, config, rng);
        for (const auto& group : genome.groups) {
            for (float a : group.angles) {
                CHECK(a >= -PI);
                CHECK(a <= PI);
            }
        }
    }
}

// --- Crossover ---

TEST_CASE("Crossover produces genome with correct structure", "[morphology]") {
    auto config = make_two_group_config();
    auto parent1 = create_default_morphology(config);
    auto parent2 = create_default_morphology(config);

    std::mt19937 rng(42);
    auto child = crossover_morphology(parent1, parent2, rng);

    REQUIRE(child.groups.size() == 2);
    CHECK(child.groups[0].angles.size() == 16);
    CHECK(child.groups[1].angles.size() == 4);
}

TEST_CASE("Crossover values come from one parent or the other", "[morphology]") {
    auto config = make_two_group_config();
    auto parent1 = create_default_morphology(config);
    auto parent2 = create_default_morphology(config);

    // Give parents distinctly different values
    for (auto& f : parent1.groups[0].arc_fracs) f = 1.0f;
    for (auto& f : parent2.groups[0].arc_fracs) f = 5.0f;

    std::mt19937 rng(42);
    auto child = crossover_morphology(parent1, parent2, rng);

    for (float f : child.groups[0].arc_fracs) {
        CHECK((f == 1.0f || f == 5.0f));
    }
}

TEST_CASE("Crossover mixes values from both parents", "[morphology]") {
    auto config = make_two_group_config();
    auto parent1 = create_default_morphology(config);
    auto parent2 = create_default_morphology(config);

    for (auto& f : parent1.groups[0].arc_fracs) f = 1.0f;
    for (auto& f : parent2.groups[0].arc_fracs) f = 5.0f;

    std::mt19937 rng(42);
    auto child = crossover_morphology(parent1, parent2, rng);

    bool has_from_p1 = false;
    bool has_from_p2 = false;
    for (float f : child.groups[0].arc_fracs) {
        if (f == 1.0f) has_from_p1 = true;
        if (f == 5.0f) has_from_p2 = true;
    }
    // With 16 eyes and 50/50, extremely unlikely to get all from one parent
    CHECK(has_from_p1);
    CHECK(has_from_p2);
}

// --- JSON round-trip ---

static std::string find_data_dir() {
    for (auto dir = std::filesystem::current_path(); dir != dir.root_path(); dir = dir.parent_path()) {
        if (std::filesystem::exists(dir / "data" / "simple_boid.json"))
            return (dir / "data").string();
    }
    return "data";
}

TEST_CASE("Morphology genome JSON round-trip", "[morphology]") {
    // Load a real boid spec, attach a morphology genome, save, reload, compare
    std::string data_dir = find_data_dir();
    BoidSpec spec = load_boid_spec(data_dir + "/simple_boid.json");

    auto config = make_two_group_config();
    auto morpho = create_default_morphology(config);
    // Modify some values so we can verify they survive the round-trip
    morpho.groups[0].angles[0] = 0.5f;
    morpho.groups[0].arc_fracs[0] = 2.5f;
    morpho.groups[1].angles[2] = -1.2f;
    morpho.groups[1].arc_fracs[3] = 0.3f;

    spec.morphology_genome = morpho;

    std::string tmp_path = data_dir + "/../build/test_morpho_roundtrip.json";
    save_boid_spec(spec, tmp_path);

    BoidSpec loaded = load_boid_spec(tmp_path);
    REQUIRE(loaded.morphology_genome.has_value());

    const auto& lm = *loaded.morphology_genome;
    REQUIRE(lm.groups.size() == 2);
    CHECK(lm.groups[0].angles.size() == 16);
    CHECK(lm.groups[1].angles.size() == 4);

    CHECK_THAT(lm.groups[0].angles[0], Catch::Matchers::WithinAbs(0.5f, 0.001f));
    CHECK_THAT(lm.groups[0].arc_fracs[0], Catch::Matchers::WithinAbs(2.5f, 0.001f));
    CHECK_THAT(lm.groups[1].angles[2], Catch::Matchers::WithinAbs(-1.2f, 0.001f));
    CHECK_THAT(lm.groups[1].arc_fracs[3], Catch::Matchers::WithinAbs(0.3f, 0.001f));

    // Clean up
    std::filesystem::remove(tmp_path);
}

TEST_CASE("BoidSpec without morphology genome loads fine", "[morphology]") {
    std::string data_dir = find_data_dir();
    BoidSpec spec = load_boid_spec(data_dir + "/simple_boid.json");
    CHECK_FALSE(spec.morphology_genome.has_value());
}
