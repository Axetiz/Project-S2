// OrbitSimLite - Body definition
//
// A Body represents a single point mass in 2D with rendering attributes.
// It is intentionally minimal: physical and visual properties are stored
// together to keep the demo compact, but in a larger project you would
// typically separate simulation state from rendering state.
#pragma once

#include <cstdint>
#include <string>
#include "vec2.hpp"

namespace orbitsimlite {

struct Body {
    double mass;         // Mass in kilograms.
    double radius;       // Visual radius in pixels (rendering only).
    Vec2 pos;            // Position in world space (metres).
    Vec2 vel;            // Velocity (metres / second).
    Vec2 acc;            // Acceleration (metres / second^2), updated per step.
    std::uint32_t color; // Packed RGB colour in 0xRRGGBB format.

    bool is_satellite;   // True for natural satellites (e.g., Moon).
    bool is_star;        // True for stars (e.g., Sun); used by collision logic.

    // Optional display name used in JSON output and debug logging. When empty,
    // JSON serialisation falls back to a synthetic "body_<index>" identifier.
    std::string name;

    Body();
    Body(double mass_, const Vec2& pos_, const Vec2& vel_, double radius_, std::uint32_t color_, bool is_satellite_ = false, bool is_star_ = false, const std::string& name_ = {});
};

} // namespace orbitsimlite
