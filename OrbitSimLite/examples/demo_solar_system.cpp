// OrbitSimLite - interactive explorer demo
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "renderer.hpp"
#include "simulator.hpp"
#include "utils.hpp"

using namespace orbitsimlite;

namespace {

ScenarioPreset make_solar_system_preset(double multiplier) {
    ScenarioPreset preset;
    preset.name = "Solar System";
    preset.gravity = Physics::DefaultG;
    preset.dt = 36000.0 * multiplier;
    preset.integrator = Integrator::RK4;
    preset.substeps = 12;
    preset.meters_to_pixels = 2e-9;

    constexpr double pi = 3.14159265358979323846;
    Vec2 earth_pos;
    Vec2 earth_vel;

    preset.bodies.emplace_back(1.989e30, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 30.0, rgb_u32(255, 255, 0), false, true, "Sun");

    {
        const double R = 5.79e10;
        const double v = 47360.0;
        const double theta = 0.0;
        preset.bodies.emplace_back(3.301e23, Vec2{R * std::cos(theta), R * std::sin(theta)},
                                   Vec2{-v * std::sin(theta), v * std::cos(theta)},
                                   6.0, rgb_u32(180, 180, 180), false, false, "Mercury");
    }

    {
        const double R = 1.082e11;
        const double v = 35020.0;
        const double theta = 2.0 * pi / 3.0;
        preset.bodies.emplace_back(4.867e24, Vec2{R * std::cos(theta), R * std::sin(theta)},
                                   Vec2{-v * std::sin(theta), v * std::cos(theta)},
                                   9.0, rgb_u32(255, 200, 120), false, false, "Venus");
    }

    {
        const double R = 2.0e11;
        const double v = 22000.0;
        const double theta = pi / 4.0;
        preset.bodies.emplace_back(5.0e24, Vec2{R * std::cos(theta), R * std::sin(theta)},
                                   Vec2{-v * std::sin(theta), v * std::cos(theta)},
                                   8.0, rgb_u32(0, 250, 180), false, false, "Blop");
    }

    {
        const double R = 1.496e11;
        const double v = 29783.0;
        const double theta = 4.0 * pi / 3.0;
        earth_pos = Vec2{R * std::cos(theta), R * std::sin(theta)};
        earth_vel = Vec2{-v * std::sin(theta), v * std::cos(theta)};
        preset.bodies.emplace_back(5.972e24, earth_pos, earth_vel, 10.0, rgb_u32(70, 120, 255), false, false, "Earth");
    }

    {
        const double moon_dist = 3.84e8;
        const double moon_speed = 1022.0;
        const double theta_earth = 4.0 * pi / 3.0;
        Vec2 t_hat(-std::sin(theta_earth), std::cos(theta_earth));
        Vec2 moon_pos = earth_pos + moon_dist * t_hat;
        Vec2 moon_vel = earth_vel + moon_speed * t_hat;
        preset.bodies.emplace_back(7.35e22, moon_pos, moon_vel, 3.0, rgb_u32(200, 200, 200), true, false, "Moon");
    }

    {
        const double R = 2.279e11;
        const double v = 24077.0;
        const double theta = pi / 2.0;
        preset.bodies.emplace_back(6.417e23, Vec2{R * std::cos(theta), R * std::sin(theta)},
                                   Vec2{-v * std::sin(theta), v * std::cos(theta)},
                                   7.0, rgb_u32(255, 100, 80), false, false, "Mars");
    }

    return preset;
}

ScenarioPreset make_binary_stars_preset(double multiplier) {
    ScenarioPreset preset;
    preset.name = "Binary Stars";
    preset.gravity = Physics::DefaultG;
    preset.dt = 3600.0 * multiplier;
    preset.integrator = Integrator::RK4;
    preset.substeps = 20;
    preset.meters_to_pixels = 2e-10;

    const double mass_sun = 1.989e30;
    const double dist = 3.0e11;
    const double v = std::sqrt(Physics::DefaultG * mass_sun / (4.0 * dist));

    preset.bodies.emplace_back(mass_sun, Vec2{-dist, 0.0}, Vec2{0.0, v}, 30.0, rgb_u32(255, 220, 120), false, true, "SunA");
    preset.bodies.emplace_back(mass_sun, Vec2{dist, 0.0}, Vec2{0.0, -v}, 30.0, rgb_u32(255, 240, 180), false, true, "SunB");
    return preset;
}

ScenarioPreset make_figure_eight_preset(double multiplier) {
    ScenarioPreset preset;
    preset.name = "Figure Eight";
    preset.gravity = 1.0;
    preset.dt = 0.001 * multiplier;
    preset.integrator = Integrator::RK4;
    preset.substeps = 4;
    preset.meters_to_pixels = 250.0;

    preset.bodies.emplace_back(1.0, Vec2{0.97000436, -0.24308753}, Vec2{0.4662036850, 0.4323657300}, 8.0, rgb_u32(255, 200, 120), false, false, "BodyA");
    preset.bodies.emplace_back(1.0, Vec2{-0.97000436, 0.24308753}, Vec2{0.4662036850, 0.4323657300}, 8.0, rgb_u32(120, 220, 255), false, false, "BodyB");
    preset.bodies.emplace_back(1.0, Vec2{0.0, 0.0}, Vec2{-0.93240737, -0.86473146}, 8.0, rgb_u32(200, 120, 255), false, false, "BodyC");
    return preset;
}

ScenarioPreset make_sandbox_preset(double multiplier) {
    ScenarioPreset preset;
    preset.name = "Sandbox";
    preset.gravity = Physics::DefaultG;
    preset.dt = 7200.0 * multiplier;
    preset.integrator = Integrator::RK4;
    preset.substeps = 8;
    preset.meters_to_pixels = 3e-9;

    preset.bodies.emplace_back(1.5e30, Vec2{0.0, 0.0}, Vec2{0.0, 0.0}, 26.0, rgb_u32(255, 235, 150), false, true, "Core");
    preset.bodies.emplace_back(7.0e24, Vec2{9.0e10, 0.0}, Vec2{0.0, 33500.0}, 9.0, rgb_u32(120, 190, 255), false, false, "Alpha");
    preset.bodies.emplace_back(4.5e24, Vec2{-1.5e11, 0.0}, Vec2{0.0, -25000.0}, 8.0, rgb_u32(255, 140, 130), false, false, "Beta");
    preset.bodies.emplace_back(2.0e23, Vec2{0.0, 2.3e11}, Vec2{-18000.0, 0.0}, 5.0, rgb_u32(180, 255, 170), false, false, "Gamma");
    return preset;
}

} // namespace

int main() {
    double multiplier = 1.0;
    std::cout << "Enter explorer speed multiplier: ";
    std::cin >> multiplier;
    if (!std::cin || multiplier <= 0.0) {
        multiplier = 1.0;
    }

    std::vector<ScenarioPreset> presets;
    presets.push_back(make_solar_system_preset(multiplier));
    presets.push_back(make_binary_stars_preset(multiplier));
    presets.push_back(make_figure_eight_preset(multiplier));
    presets.push_back(make_sandbox_preset(multiplier));

    const ScenarioPreset& initial = presets.front();
    Simulator sim(initial.gravity, initial.dt, initial.integrator);
    sim.set_substeps(initial.substeps);
    sim.set_bodies(initial.bodies);

    Renderer renderer(1280, 840, initial.meters_to_pixels);
    renderer.set_presets(presets);
    renderer.run(sim);

    return 0;
}
