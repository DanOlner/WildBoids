#pragma once

#include <cmath>
#include <vector>

enum class EntityFilter { Prey, Predator, Any, Food, Speed, AngularVelocity, Noise };
enum class SignalType { NearestDistance, SectorDensity };

// Channels for compound-eye sensors
enum class SensorChannel { Food, Same, Opposite };

struct SensorSpec {
    int id = 0;
    float center_angle = 0;   // offset from boid heading (body frame, radians)
    float arc_width = 0;      // total angular width (radians)
    float max_range = 0;      // detection radius (world units)
    EntityFilter filter = EntityFilter::Any;
    SignalType signal_type = SignalType::NearestDistance;
};

// A single compound eye: arc position + range, produces multiple channel outputs
struct EyeSpec {
    int id = 0;
    float center_angle = 0;   // radians, body frame
    float arc_width = 0;      // radians
    float max_range = 0;      // world units
};

// Full compound-eye sensor configuration
struct CompoundEyeConfig {
    std::vector<EyeSpec> eyes;              // short-range compound eyes
    std::vector<EyeSpec> long_range_eyes;   // long-range narrow eyes (second tier)
    std::vector<SensorChannel> channels;    // which channels are active (shared by both tiers)
    bool has_speed_sensor = true;
    bool has_angular_velocity_sensor = false;
    bool has_noise_sensor = false;
    bool has_shoaling_sensor = false;
    bool has_hunger_sensor = false;

    int short_range_eye_count() const { return static_cast<int>(eyes.size()); }
    int long_range_eye_count() const { return static_cast<int>(long_range_eyes.size()); }

    int total_inputs() const {
        return static_cast<int>((eyes.size() + long_range_eyes.size()) * channels.size())
               + (has_speed_sensor ? 1 : 0)
               + (has_angular_velocity_sensor ? 1 : 0)
               + (has_noise_sensor ? 1 : 0)
               + (has_shoaling_sensor ? 1 : 0)
               + (has_hunger_sensor ? 1 : 0);
    }
};

// Check whether an angle falls within a symmetric arc around center_angle.
// All angles in radians. Handles ±π wrapping.
inline bool angle_in_arc(float angle, float center, float arc_width) {
    float diff = angle - center;
    // Wrap to [-π, π]
    diff = std::fmod(diff + static_cast<float>(M_PI), 2.0f * static_cast<float>(M_PI));
    if (diff < 0) diff += 2.0f * static_cast<float>(M_PI);
    diff -= static_cast<float>(M_PI);
    return std::abs(diff) <= arc_width * 0.5f;
}
