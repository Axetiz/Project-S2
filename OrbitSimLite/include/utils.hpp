// OrbitSimLite - Utility helpers
//
// Small helper routines for colour packing/unpacking, unit conversions and
// deterministic pseudo-random colour generation. These are kept free of any
// simulation state so they can be reused in other contexts.
#pragma once

#include <cstdint>

namespace orbitsimlite {

// Pack RGB components into 0xRRGGBB
constexpr std::uint32_t rgb_u32(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return (static_cast<std::uint32_t>(r) << 16) |
           (static_cast<std::uint32_t>(g) << 8) |
           (static_cast<std::uint32_t>(b));
}

// Unpack 0xRRGGBB into components
inline void unpack_rgb(std::uint32_t color, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    r = static_cast<std::uint8_t>((color >> 16) & 0xFF);
    g = static_cast<std::uint8_t>((color >> 8) & 0xFF);
    b = static_cast<std::uint8_t>(color & 0xFF);
}

// Unit conversions given a constant scale (meters to pixels)
inline double meters_to_pixels(double meters, double scale) { return meters * scale; }
inline double pixels_to_meters(double pixels, double scale) { return pixels / scale; }

// Simple hash-based deterministic pseudo-random color from an integer seed
inline std::uint32_t random_color_u32(std::uint32_t seed) {
    std::uint32_t x = seed * 1664525u + 1013904223u;
    std::uint8_t r = static_cast<std::uint8_t>((x >> 16) & 0xFF);
    std::uint8_t g = static_cast<std::uint8_t>((x >> 8) & 0xFF);
    std::uint8_t b = static_cast<std::uint8_t>((x) & 0xFF);
    // Avoid too dark colors
    r = static_cast<std::uint8_t>(128 + (r >> 1));
    g = static_cast<std::uint8_t>(128 + (g >> 1));
    b = static_cast<std::uint8_t>(128 + (b >> 1));
    return rgb_u32(r, g, b);
}

} // namespace orbitsimlite
