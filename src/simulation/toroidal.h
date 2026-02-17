#pragma once

#include "simulation/vec2.h"

// Shortest-path vector from `from` to `to` on a toroidal surface.
// This is the single point of truth for toroidal wrapping math.
inline Vec2 toroidal_delta(Vec2 from, Vec2 to, float world_w, float world_h) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;

    if (dx >  world_w * 0.5f) dx -= world_w;
    if (dx < -world_w * 0.5f) dx += world_w;
    if (dy >  world_h * 0.5f) dy -= world_h;
    if (dy < -world_h * 0.5f) dy += world_h;

    return {dx, dy};
}

// Squared toroidal distance — avoids sqrt for range checks.
inline float toroidal_distance_sq(Vec2 from, Vec2 to, float world_w, float world_h) {
    return toroidal_delta(from, to, world_w, world_h).length_squared();
}
