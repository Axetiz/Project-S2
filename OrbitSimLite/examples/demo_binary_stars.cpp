// OrbitSimLite - Demo 2: two massive bodies (two suns)
#include <cstdint>
#include <iostream>
#include <cmath>
#include "simulator.hpp"
#include "renderer.hpp"
#include "utils.hpp"

using namespace orbitsimlite;

int main() {
    double multiplier = 1.0;
    std::cout << "Enter simulator speed multiplier for two-suns demo: ";
    std::cin >> multiplier;

    // Use a moderate timestep with substeps
    Simulator sim(Physics::DefaultG, 3600.0 * multiplier, Integrator::RK4);
    sim.set_substeps(20);

    // Two equal-mass stars
    double mass_sun = 1.989e30;
    // Half of center-to-center distance. With radius = 30 px and scale = 2e-10,
    // dist = 3e11 gives roughly one sun diameter gap between the two suns.
    double dist = 3.0e11; // separation / 2

    // Circular binary orbit:
    // each star orbits barycenter at radius r = dist, separation = 2*dist
    // For equal masses: v = sqrt(G * m / (4 * r))
    double G = Physics::DefaultG;
    double r = dist;
    double v = std::sqrt(G * mass_sun / (4.0 * r));

    // Left star: at (-dist, 0), velocity upward
    Body sunA(mass_sun, Vec2{-dist, 0.0}, Vec2{0.0,  v}, 30.0, rgb_u32(255, 220, 120), false, true, "SunA");
    sim.add_body(sunA);

    // Right star: at (+dist, 0), velocity downward
    Body sunB(mass_sun, Vec2{ dist, 0.0}, Vec2{0.0, -v}, 30.0, rgb_u32(255, 240, 180), false, true, "SunB");
    sim.add_body(sunB);

    Renderer renderer(1000, 800, 2e-10);
    renderer.run(sim);

    return 0;
}
