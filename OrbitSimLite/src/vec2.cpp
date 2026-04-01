// OrbitSimLite - Vec2 implementation
#include "vec2.hpp"

namespace orbitsimlite {

Vec2::Vec2() : x(0.0), y(0.0) {}
Vec2::Vec2(double x_, double y_) : x(x_), y(y_) {}

Vec2 Vec2::operator+(const Vec2& other) const { return Vec2{x + other.x, y + other.y}; }
Vec2 Vec2::operator-(const Vec2& other) const { return Vec2{x - other.x, y - other.y}; }
Vec2 Vec2::operator*(double s) const { return Vec2{x * s, y * s}; }
Vec2 Vec2::operator/(double s) const { return Vec2{x / s, y / s}; }

Vec2& Vec2::operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
Vec2& Vec2::operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
Vec2& Vec2::operator*=(double s) { x *= s; y *= s; return *this; }
Vec2& Vec2::operator/=(double s) { x /= s; y /= s; return *this; }

double Vec2::length() const { return std::sqrt(x * x + y * y); }
double Vec2::length_squared() const { return x * x + y * y; }

Vec2 Vec2::normalized() const {
    double len = length();
    if (len == 0.0) return Vec2{0.0, 0.0};
    return Vec2{x / len, y / len};
}

double Vec2::dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }

double Vec2::distance(const Vec2& a, const Vec2& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

Vec2 operator*(double s, const Vec2& v) { return Vec2{v.x * s, v.y * s}; }

} // namespace orbitsimlite