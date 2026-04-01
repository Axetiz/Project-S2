// OrbitSimLite - umbrella header
//
// Including this header pulls in the public C++ interface of the library:
//  - version information
//  - 2D vector math (Vec2)
//  - Body, Physics, Simulator (core physics)
//  - optional SFML renderer and utility helpers
//
// Typical usage:
//   #include <orbitsimlite/orbitsimlite.hpp>
//   orbitsimlite::Simulator sim(...);
//
#pragma once

#include <orbitsimlite/version.hpp>

#include "vec2.hpp"
#include "body.hpp"
#include "physics.hpp"
#include "simulator.hpp"
#include "renderer.hpp"
#include "utils.hpp"

