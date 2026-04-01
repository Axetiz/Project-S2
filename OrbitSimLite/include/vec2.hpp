// OrbitSimLite - 2D vector math
//
// A lightweight 2D vector type with the minimal set of operations required
// by the simulation. This is intentionally simple and header-only for the
// interface, with implementation in src/vec2.cpp to keep build times low.
#pragma once

#include <cmath>

namespace orbitsimlite {

struct Vec2 {
    double x;
    double y;

    Vec2();
    Vec2(double x_, double y_);

    // Basic arithmetic
    Vec2 operator+(const Vec2& other) const;
    Vec2 operator-(const Vec2& other) const;
    Vec2 operator*(double s) const;
    Vec2 operator/(double s) const;

    Vec2& operator+=(const Vec2& other);
    Vec2& operator-=(const Vec2& other);
    Vec2& operator*=(double s);
    Vec2& operator/=(double s);

    // Magnitudes
    double length() const;
    double length_squared() const;
    Vec2 normalized() const; // returns zero vector if length is 0

    // Vector utilities
    static double dot(const Vec2& a, const Vec2& b);
    static double distance(const Vec2& a, const Vec2& b);
};

// Scalar on the left
Vec2 operator*(double s, const Vec2& v);

} // namespace orbitsimlite
