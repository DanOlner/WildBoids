#pragma once

#include <cmath>

enum class EntityFilter { Prey, Predator, Any };
enum class SignalType { NearestDistance, SectorDensity };

struct SensorSpec {
    int id = 0;
    float center_angle = 0;   // offset from boid heading (body frame, radians)
    float arc_width = 0;      // total angular width (radians)
    float max_range = 0;      // detection radius (world units)
    EntityFilter filter = EntityFilter::Any;
    SignalType signal_type = SignalType::NearestDistance;
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
