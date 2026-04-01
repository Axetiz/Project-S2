// OrbitSimLite - Demo application
#include <cstdint>
#include <iostream>
#include <cmath>
#include "simulator.hpp"
#include "renderer.hpp"
#include "utils.hpp"

using namespace orbitsimlite;

int main() {
    // Create simulator: G in SI, dt scaled by multiplier
    double multiplier = 1.0;
    std::cout << "Enter standard simulator speed multiplier: ";
    std::cin >> multiplier;

    Simulator sim(Physics::DefaultG, 36000.0 * multiplier, Integrator::RK4);
    sim.set_substeps(12); // 12 substeps -> 3000s / 12 internal step

    // Bodies

    // Sun
    Body sun(1.989e30, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 30.0, rgb_u32(255, 255, 0), false, true, "Sun");
    sim.add_body(sun);

    constexpr double pi = 3.14159265358979323846;

    // Mercury (angle 0 rad)
    {
        double R = 5.79e10;
        double v = 47360.0;
        double theta = 0.0;
        double c = std::cos(theta);
        double s = std::sin(theta);
        Vec2 pos(R * c, R * s);
        Vec2 vel(-v * s, v * c);
        Body mercury(3.301e23, pos, vel, 6.0, rgb_u32(180, 180, 180), false, false, "Mercury");
        sim.add_body(mercury);
    }

    // Venus (angle 2*pi/3)
    Vec2 earth_pos;
    Vec2 earth_vel;
    {
        double R = 1.082e11;
        double v = 35020.0;
        double theta = 2.0 * pi / 3.0;
        double c = std::cos(theta);
        double s = std::sin(theta);
        Vec2 pos(R * c, R * s);
        Vec2 vel(-v * s, v * c);
        Body venus(4.867e24, pos, vel, 9.0, rgb_u32(255, 200, 120), false, false, "Venus");
        sim.add_body(venus);
    }

    // Experimental planet (between Earth and Mars, angle pi/4)
    {
        double R = 2.0e11;
        double v = 22000.0;
        double theta = pi / 4.0;
        double c = std::cos(theta);
        double s = std::sin(theta);
        Vec2 pos(R * c, R * s);
        Vec2 vel(-v * s, v * c);
        Body blop(5.0e24, pos, vel, 8.0, rgb_u32(0, 250, 180), false, false, "Blop");
        sim.add_body(blop);
    }

    // Earth (angle 4*pi/3)
    {
        double R = 1.496e11;
        double v = 29783.0;
        double theta = 4.0 * pi / 3.0;
        double c = std::cos(theta);
        double s = std::sin(theta);
        earth_pos = Vec2(R * c, R * s);
        earth_vel = Vec2(-v * s, v * c);
        Body earth(5.972e24, earth_pos, earth_vel, 10.0, rgb_u32(70, 120, 255), false, false, "Earth");
        sim.add_body(earth);
    }

    // Moon (bind to Earth; offset along Earth's tangential direction, prograde)
    {
        double moon_dist = 3.84e8;   // Earth–Moon distance (m)
        double moon_speed = 1022.0;  // m/s relative to Earth

        // Tangential unit vector for Earth (perpendicular to Earth–Sun radius)
        double theta_earth = 4.0 * pi / 3.0;
        double c = std::cos(theta_earth);
        double s = std::sin(theta_earth);
        Vec2 t_hat(-s, c);

        Vec2 moon_pos = earth_pos + Vec2(t_hat.x * moon_dist, t_hat.y * moon_dist);
        Vec2 moon_vel = earth_vel + Vec2(t_hat.x * moon_speed, t_hat.y * moon_speed);

        Body moon(7.35e22, moon_pos, moon_vel, 3.0, rgb_u32(200, 200, 200), true, false, "Moon");
        sim.add_body(moon);
    }

    // Mars (angle pi/2)
    {
        double R = 2.279e11;
        double v = 24077.0;
        double theta = pi / 2.0;
        double c = std::cos(theta);
        double s = std::sin(theta);
        Vec2 pos(R * c, R * s);
        Vec2 vel(-v * s, v * c);
        Body mars(6.417e23, pos, vel, 7.0, rgb_u32(255, 100, 80), false, false, "Mars");
        sim.add_body(mars);
    }

    // Renderer
    Renderer renderer(1000, 800, 2e-9);
    renderer.run(sim);

    return 0;
}
