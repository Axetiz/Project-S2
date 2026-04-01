// OrbitSimLite - Physics utilities (Newtonian gravity + time integrators)
//
// This module contains the core numerical methods used by the simulator:
//  - computation of pairwise Newtonian gravitational acceleration in 2D
//  - a simple symplectic Euler integrator (good energy behaviour, low cost)
//  - an educational Runge–Kutta 4 (RK4) step for smoother trajectories
//
// Notes:
//  - All quantities are expressed in SI units (m, kg, s).
//  - The RK4 implementation treats the other bodies as fixed during the step;
//    this is acceptable for a small educational N-body demo, but is not a
//    fully coupled, production-grade N-body integrator.
//    Background reading: https://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods
#pragma once

#include <vector>
#include "body.hpp"

namespace orbitsimlite {

struct Physics {
    // Universal gravitational constant in SI units (m^3 / (kg * s^2)).
    // A slightly rounded value is sufficient for visualisation.
    static constexpr double DefaultG = 6.674e-11;

    // Compute the total gravitational acceleration acting on 'target' due to
    // all bodies in 'others' using the Newtonian point-mass model:
    //
    //   a = G * sum_i m_i * (r_i - r_target) / |r_i - r_target|^3
    //
    // where r_i is the position of body i.
    //
    // The caller is responsible for passing an 'others' vector that does not
    // contain 'target' itself (the Simulator handles this when needed).
    static Vec2 acceleration(const Body& target, const std::vector<Body>& others, double G = DefaultG);

    // Advance a single body by one explicit symplectic Euler step, given a
    // precomputed acceleration and a timestep 'dt' (in seconds).
    //
    // The integration scheme is:
    //   v_{n+1} = v_n + a_n * dt
    //   x_{n+1} = x_n + v_{n+1} * dt
    //
    // Symplectic Euler is numerically cheap and exhibits better energy
    // behaviour than plain (non-symplectic) Euler for orbital problems.
    static void step_euler(Body& body, const Vec2& acc, double dt);

    // One-step Runge–Kutta 4th order (RK4) integrator for a single body in the
    // gravitational field induced by 'others'. All bodies in 'others' are kept
    // fixed while evaluating intermediate stages (k1..k4).
    //
    // This is intentionally simplified compared to a fully coupled N-body
    // RK4 scheme, but it provides smoother orbits than Euler for a given dt
    // in the context of this teaching demo.
    //
    // Reference: https://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods
    static void step_rk4(Body& body, const std::vector<Body>& others, double G, double dt);
};

} // namespace orbitsimlite
