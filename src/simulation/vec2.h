#pragma once

#include <cmath>

struct Vec2 {
    float x = 0;
    float y = 0;

    Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }

    Vec2& operator+=(Vec2 other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(Vec2 other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    float dot(Vec2 other) const { return x * other.x + y * other.y; }
    float length_squared() const { return x * x + y * y; }
    float length() const { return std::sqrt(length_squared()); }

    Vec2 normalized() const {
        float len = length();
        if (len < 1e-8f) return {0, 0};
        return *this / len;
    }

    // Rotate this vector by angle (radians, CCW positive)
    Vec2 rotated(float angle) const {
        float c = std::cos(angle);
        float s = std::sin(angle);
        return {x * c - y * s, x * s + y * c};
    }
};

// scalar * Vec2
inline Vec2 operator*(float s, Vec2 v) { return {s * v.x, s * v.y}; }

// 2D cross product (returns scalar: the z-component of the 3D cross product)
inline float cross2d(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
