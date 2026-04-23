// OrbitSimLite - empty workspace demo for manual testing
#include <iostream>

#include "demo_cli.hpp"
#include "renderer.hpp"
#include "simulator.hpp"

using namespace orbitsimlite;
using namespace orbitsimlite_demo;

int main() {
    double multiplier = 1.0;
    std::cout << "Enter simulator speed multiplier for empty-plane demo: ";
    std::cin >> multiplier;
    if (!std::cin || multiplier <= 0.0) {
        multiplier = 1.0;
    }

    const LaunchOptions launch_options = read_launch_options();

    Simulator sim(Physics::DefaultG, 3600.0 * multiplier, Integrator::RK4);
    sim.set_substeps(8);

    Renderer renderer(1280, 840, 3e-9);
    renderer.set_output_options(OutputOptions{
        launch_options.enable_json,
        launch_options.enable_csv,
    });

    if (launch_options.enable_sfml) {
        renderer.run(sim);
    } else {
        renderer.run_headless(sim, launch_options.headless_real_time_seconds, "Simulator");
    }

    return 0;
}
