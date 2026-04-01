// OrbitSimLite - Demo 3: figure-eight three-body choreography
//
// This demo visualises the famous figure‑eight solution of the Newtonian
// three‑body problem for three equal masses. Each body has the same mass and
// follows the same figure‑eight curve, phase‑shifted by 1/3 of a period.
//
// The initial conditions below are a standard non‑dimensionalised set for
// G = 1 and m = 1 (see Chenciner–Montgomery, Annals of Mathematics 2000).
// We run the integrator in these dimensionless units and only use a scale
// factor when converting to screen coordinates.

#include <cstdint>
#include <iostream>
#include <cmath>

#include "simulator.hpp"
#include "renderer.hpp"
#include "utils.hpp"

using namespace orbitsimlite;

int main() {
    double multiplier = 1.0;
    std::cout << "Enter simulator speed multiplier for figure-eight demo: ";
    std::cin >> multiplier;

    // For this demo we use dimensionless units: set G = 1 and equal masses.
    // The timestep is kept small to preserve the fine structure of the orbit.
    // If input fails or is non‑positive, fall back to 1.0.
    if (!std::cin || multiplier <= 0.0) {
        multiplier = 1.0;
    }

    const double G_dimless = 1.0;
    const double dt = 0.001 * multiplier; // base step
    Simulator sim(G_dimless, dt, Integrator::RK4);
    sim.set_substeps(4); // effective internal step dt/4

    const double m = 1.0;

    // Standard figure‑eight initial conditions (approximate):
    // Positions
    Vec2 p1{ 0.97000436, -0.24308753};
    Vec2 p2{-0.97000436,  0.24308753};
    Vec2 p3{ 0.0,         0.0       };

    // Velocities
    Vec2 v1{ 0.4662036850,  0.4323657300};
    Vec2 v2{ 0.4662036850,  0.4323657300};
    Vec2 v3{-0.93240737,   -0.86473146 }; // - (v1 + v2)

    Body b1(m, p1, v1, 8.0, rgb_u32(255, 200, 120), false, false, "BodyA");
    Body b2(m, p2, v2, 8.0, rgb_u32(120, 220, 255), false, false, "BodyB");
    Body b3(m, p3, v3, 8.0, rgb_u32(200, 120, 255), false, false, "BodyC");

    sim.add_body(b1);
    sim.add_body(b2);
    sim.add_body(b3);

    // Scale chosen so the figure‑eight fills a good portion of the window.
    Renderer renderer(1000, 800, 250.0);
    renderer.run(sim);

    return 0;
}
