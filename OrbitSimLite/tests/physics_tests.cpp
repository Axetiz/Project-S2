// OrbitSimLite - simple numerical accuracy checks for the physics core
//
// These tests are intentionally lightweight and header-only; they are not a
// full unit test framework, but they provide a quick way to sanity‑check the
// integrators and gravitational model before publishing the project.
//
// Usage (from the CMake build directory):
//   cmake --build . --target orbitsimlite_tests
//   ./orbitsimlite_tests

#include <cmath>
#include <iostream>

#include "physics.hpp"
#include "simulator.hpp"

using namespace orbitsimlite;

namespace {

constexpr double kTolAccel = 1e-10;   // relative tolerance for acceleration
constexpr double kTolRadius = 1e-2;   // 1% relative radius tolerance
constexpr double kTolEnergy = 5e-3;   // 0.5% relative energy tolerance
constexpr double kTolPos = 1e-3;      // 0.1% relative position tolerance
constexpr double kTolMomentum = 5e-3; // 0.5% relative momentum tolerance

// Helper to compute relative error with basic protection against division by 0.
double rel_error(double value, double reference) {
    const double denom = std::max(std::abs(reference), 1e-15);
    return std::abs(value - reference) / denom;
}

bool test_newtonian_accel_single_mass() {
    // Analytic case: a light body at (r, 0) under the gravity of a single
    // massive body at the origin. Expected acceleration:
    //   a = (-G * M / r^2, 0).
    Body massive(1.0e25, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);
    Body target(1.0, Vec2{1.0e8, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);

    std::vector<Body> others{massive};
    Vec2 a = Physics::acceleration(target, others, Physics::DefaultG);

    const double r = 1.0e8;
    const double a_ref = -Physics::DefaultG * massive.mass / (r * r);

    double ex = rel_error(a.x, a_ref);
    double ey = std::abs(a.y); // should be ~0

    return ex < kTolAccel && ey < kTolAccel * std::abs(a_ref);
}

bool test_superposition_two_masses_zero_field_at_midpoint() {
    // Two identical masses at (-d, 0) and (+d, 0). At the origin, the
    // gravitational accelerations cancel and the net field should be ~0.
    const double M = 1.0e25;
    const double d = 1.0e8;

    Body left(M, Vec2{-d, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);
    Body right(M, Vec2{ d, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);
    Body probe(1.0, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);

    std::vector<Body> others{left, right};
    Vec2 a = Physics::acceleration(probe, others, Physics::DefaultG);

    return std::abs(a.x) < 1e-12 && std::abs(a.y) < 1e-12;
}

bool test_superposition_two_masses_on_axis() {
    // Two identical masses at (0, -d) and (0, +d) and a probe above them on
    // the Y axis. The net acceleration should point downwards (negative Y)
    // with negligible X component.
    const double M = 1.0e25;
    const double d = 1.0e8;

    Body down(M, Vec2{0.0, -d}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);
    Body up(M, Vec2{0.0,  d}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);
    Body probe(1.0, Vec2{0.0, 2.0 * d}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);

    std::vector<Body> others{down, up};
    Vec2 a = Physics::acceleration(probe, others, Physics::DefaultG);

    // Symmetry: a.x ~ 0, a.y should be negative (pointing back towards the
    // pair of masses).
    return std::abs(a.x) < 1e-12 && a.y < 0.0;
}

bool test_circular_orbit_rk4() {
    // Two-body circular orbit: a light planet of mass m orbiting a fixed sun
    // of mass M at radius r with speed v = sqrt(G * M / r).
    //
    // We only integrate the planet and keep the sun fixed via 'others'; this
    // matches how step_rk4 is used in the simulator.
    const double M = 1.989e30;
    const double m = 5.972e24;
    const double r0 = 1.496e11; // 1 AU

    Body sun(M, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFF00);
    Body planet(m, Vec2{r0, 0.0}, Vec2{0.0, 0.0}, 1.0, 0x0000FF);

    const double v0 = std::sqrt(Physics::DefaultG * M / r0);
    planet.vel = Vec2{0.0, v0};

    const double orbital_period = 2.0 * M_PI * r0 / v0;

    std::vector<Body> others{sun};

    const double dt = orbital_period / 2000.0; // 2000 steps per orbit
    const int steps = static_cast<int>(std::round(orbital_period / dt));

    const double E0 = 0.5 * m * v0 * v0 - Physics::DefaultG * M * m / r0;

    int radius_ok = 0;
    int energy_ok = 0;

    for (int i = 0; i < steps; ++i) {
        Physics::step_rk4(planet, others, Physics::DefaultG, dt);

        const double r = std::sqrt(planet.pos.x * planet.pos.x +
                                   planet.pos.y * planet.pos.y);
        const double v2 = planet.vel.x * planet.vel.x +
                          planet.vel.y * planet.vel.y;
        const double E = 0.5 * m * v2 - Physics::DefaultG * M * m / r;

        if (rel_error(r, r0) < kTolRadius) {
            ++radius_ok;
        }
        if (rel_error(E, E0) < kTolEnergy) {
            ++energy_ok;
        }
    }

    const double radius_ratio =
        static_cast<double>(radius_ok) / static_cast<double>(steps);
    const double energy_ratio =
        static_cast<double>(energy_ok) / static_cast<double>(steps);

    std::cout << "[RK4 circular orbit] radius within "
              << (kTolRadius * 100.0)
              << "% for " << radius_ok << " / " << steps
              << " steps (" << radius_ratio * 100.0 << "%)\n";

    std::cout << "[RK4 circular orbit] energy within "
              << (kTolEnergy * 100.0)
              << "% for " << energy_ok << " / " << steps
              << " steps (" << energy_ratio * 100.0 << "%)\n";

    // Require at least 95% of steps to be within tolerance for both metrics.
    return radius_ratio > 0.95 && energy_ratio > 0.95;
}

bool test_rk4_vs_euler_error() {
    // Same circular orbit as above, but compare global position error after
    // one period between Euler and RK4. RK4 should be noticeably more accurate.
    const double M = 1.989e30;
    const double m = 5.972e24;
    const double r0 = 1.496e11;

    const double v0 = std::sqrt(Physics::DefaultG * M / r0);
    const double T = 2.0 * M_PI * r0 / v0;
    const double dt = T / 2000.0;
    const int steps = static_cast<int>(std::round(T / dt));

    Body sun(M, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFF00);
    std::vector<Body> others{sun};

    Body euler(m, Vec2{r0, 0.0}, Vec2{0.0, v0}, 1.0, 0xFFFFFF);
    Body rk4(m, Vec2{r0, 0.0}, Vec2{0.0, v0}, 1.0, 0xFFFFFF);

    for (int i = 0; i < steps; ++i) {
        Vec2 acc_e = Physics::acceleration(euler, others, Physics::DefaultG);
        Physics::step_euler(euler, acc_e, dt);
        Physics::step_rk4(rk4, others, Physics::DefaultG, dt);
    }

    double r_e = std::sqrt(euler.pos.x * euler.pos.x + euler.pos.y * euler.pos.y);
    double r_r = std::sqrt(rk4.pos.x * rk4.pos.x + rk4.pos.y * rk4.pos.y);

    double err_e = rel_error(r_e, r0);
    double err_r = rel_error(r_r, r0);

    std::cout << "[Euler vs RK4] radius error Euler=" << err_e
              << ", RK4=" << err_r << "\n";

    return err_r < err_e;
}

bool test_momentum_conservation_two_body() {
    // Two equal masses with opposite velocities; total linear momentum should
    // remain approximately zero over a short integration.
    const double M = 1.0e25;

    Body a(M, Vec2{-1.0e8, 0.0}, Vec2{0.0, 1.0e3}, 1.0, 0xFFFFFF);
    Body b(M, Vec2{ 1.0e8, 0.0}, Vec2{0.0,-1.0e3}, 1.0, 0xFFFFFF);

    Simulator sim(Physics::DefaultG, 10.0, Integrator::RK4);
    sim.set_substeps(10);
    sim.add_body(a);
    sim.add_body(b);

    const int steps = 1000;

    auto total_momentum = [](const std::vector<Body>& bs) {
        Vec2 p{0.0, 0.0};
        for (const auto& b : bs) {
            p.x += b.mass * b.vel.x;
            p.y += b.mass * b.vel.y;
        }
        return p;
    };

    const auto& initial_bodies = sim.get_bodies();
    Vec2 p0 = total_momentum(initial_bodies);

    for (int i = 0; i < steps; ++i) {
        sim.step();
    }

    const auto& final_bodies = sim.get_bodies();
    Vec2 p1 = total_momentum(final_bodies);

    double p0_mag = std::sqrt(p0.x * p0.x + p0.y * p0.y);
    double p1_mag = std::sqrt(p1.x * p1.x + p1.y * p1.y);

    double err = rel_error(p1_mag, p0_mag);
    return err < kTolMomentum;
}

bool test_simulator_time_accumulation() {
    Simulator sim(Physics::DefaultG, 2.5, Integrator::Euler);
    sim.set_substeps(4);

    // Add a dummy body so that the simulator actually performs a step and
    // advances its internal time counter.
    Body dummy(1.0, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);
    sim.add_body(dummy);

    double t0 = sim.get_time();
    sim.step();
    double t1 = sim.get_time();
    sim.step();
    double t2 = sim.get_time();

    // Allow for tiny floating‑point noise even though 2.5 is exactly
    // representable in binary64.
    return (std::abs(t0) < 1e-12) &&
           (std::abs(t1 - 2.5) < 1e-9) &&
           (std::abs(t2 - 5.0) < 1e-9);
}

bool test_substeps_equivalence() {
    // With substeps, the effective internal step is dt / n. Here we compare a
    // single-step integration with (dt, substeps=n) against n steps of size
    // dt/n. The final position should be close.
    Simulator sim_sub(Physics::DefaultG, 10.0, Integrator::Euler);
    Simulator sim_ref(Physics::DefaultG, 10.0 / 10.0, Integrator::Euler);

    sim_sub.set_substeps(10);
    sim_ref.set_substeps(1);

    Body b1(1.0, Vec2{0.0, 0.0}, Vec2{1.0, 0.0}, 1.0, 0xFFFFFF);
    Body b2 = b1;

    sim_sub.add_body(b1);
    sim_ref.add_body(b2);

    sim_sub.step();
    for (int i = 0; i < 10; ++i) {
        sim_ref.step();
    }

    const auto& s1 = sim_sub.get_bodies()[0];
    const auto& s2 = sim_ref.get_bodies()[0];

    double err = rel_error(s1.pos.x, s2.pos.x);
    return err < kTolPos;
}

bool test_fixed_body_does_not_move() {
    // A heavy body at the origin and a lighter body nearby. Over a short
    // integration the heavy body should move much less than the light body.
    Body fixed(1.0e26, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);
    Body mover(1.0e20, Vec2{1.0e7, 0.0}, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);

    Simulator sim(Physics::DefaultG, 1.0, Integrator::Euler);
    sim.set_substeps(10);
    sim.add_body(fixed);
    sim.add_body(mover);

    for (int i = 0; i < 1000; ++i) {
        sim.step();
    }

    const auto& bodies = sim.get_bodies();
    const auto& fixed_final = bodies[0];

    return std::abs(fixed_final.pos.x) < 1e-9 &&
           std::abs(fixed_final.pos.y) < 1e-9;
}

bool test_satellite_flag_preserved() {
    Body moon(7.35e22, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 3.0,
              0xCCCCCC, true, false, "Moon");

    Simulator sim(Physics::DefaultG, 1.0, Integrator::Euler);
    sim.add_body(moon);
    sim.step();

    const auto& bodies = sim.get_bodies();
    return bodies[0].is_satellite && !bodies[0].is_star;
}

} // namespace

int main() {
    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*fn)()) {
        bool ok = fn();
        std::cout << "[TEST] " << name << ": " << (ok ? "OK" : "FAIL") << "\n";
        if (ok) ++passed;
        ++total;
    };

    run("newtonian_accel_single_mass", &test_newtonian_accel_single_mass);
    run("superposition_zero_field_midpoint", &test_superposition_two_masses_zero_field_at_midpoint);
    run("superposition_on_axis", &test_superposition_two_masses_on_axis);
    run("circular_orbit_rk4", &test_circular_orbit_rk4);
    run("rk4_vs_euler_error", &test_rk4_vs_euler_error);
    run("momentum_conservation_two_body", &test_momentum_conservation_two_body);
    run("simulator_time_accumulation", &test_simulator_time_accumulation);
    run("substeps_equivalence", &test_substeps_equivalence);
    run("fixed_body_does_not_move", &test_fixed_body_does_not_move);
    run("satellite_flag_preserved", &test_satellite_flag_preserved);

    double percent = 100.0 * static_cast<double>(passed) /
                     static_cast<double>(total);

    std::cout << "\nPhysics tests passed: " << passed << " / " << total
              << " (" << percent << "%)\n";

    return (passed == total) ? 0 : 1;
}
