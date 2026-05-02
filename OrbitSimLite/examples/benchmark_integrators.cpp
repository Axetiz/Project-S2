// OrbitSimLite - simple integrator runtime benchmark
//
// This benchmark is intentionally lightweight. Its purpose is not to produce
// rigorous scientific performance claims, but to give quick engineering data
// for the project report and for comparing Euler vs RK4 on the same workload.

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "simulator.hpp"

using namespace orbitsimlite;

namespace {

struct BenchmarkResult {
    std::size_t body_count {0};
    Integrator integrator {Integrator::Euler};
    int steps {0};
    int repeats {0};
    double average_ms {0.0};
    double ms_per_step {0.0};
};

std::vector<Body> make_benchmark_bodies(std::size_t count) {
    // Keep the generator deterministic so repeated benchmark runs are easier
    // to compare across code changes.
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> position_dist(-5.0e6, 5.0e6);
    std::uniform_real_distribution<double> velocity_dist(-150.0, 150.0);
    std::uniform_real_distribution<double> mass_dist(5.0e18, 5.0e20);

    std::vector<Body> bodies;
    bodies.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        bodies.emplace_back(
            mass_dist(rng),
            Vec2{position_dist(rng), position_dist(rng)},
            Vec2{velocity_dist(rng), velocity_dist(rng)},
            3.0,
            0xFFFFFF,
            false,
            false,
            "B" + std::to_string(i));
    }
    return bodies;
}

BenchmarkResult run_benchmark(std::size_t body_count,
                              Integrator integrator,
                              int steps,
                              int repeats) {
    BenchmarkResult result;
    result.body_count = body_count;
    result.integrator = integrator;
    result.steps = steps;
    result.repeats = repeats;

    const std::vector<Body> seed_bodies = make_benchmark_bodies(body_count);
    double accumulated_ms = 0.0;

    for (int repeat = 0; repeat < repeats; ++repeat) {
        Simulator sim(Physics::DefaultG, 0.25, integrator);
        sim.set_substeps(1);
        sim.set_bodies(seed_bodies);

        const auto start = std::chrono::steady_clock::now();
        for (int step = 0; step < steps; ++step) {
            sim.step();
        }
        const auto end = std::chrono::steady_clock::now();

        accumulated_ms +=
            std::chrono::duration<double, std::milli>(end - start).count();
    }

    result.average_ms = accumulated_ms / static_cast<double>(repeats);
    result.ms_per_step = result.average_ms / static_cast<double>(steps);
    return result;
}

std::string integrator_name(Integrator integrator) {
    return integrator == Integrator::Euler ? "Euler" : "RK4";
}

void print_result_row(const BenchmarkResult& result) {
    std::cout << std::left
              << std::setw(10) << result.body_count
              << std::setw(12) << integrator_name(result.integrator)
              << std::setw(10) << result.steps
              << std::setw(10) << result.repeats
              << std::setw(16) << std::fixed << std::setprecision(3) << result.average_ms
              << std::setw(16) << std::fixed << std::setprecision(5) << result.ms_per_step
              << '\n';
}

} // namespace

int main() {
    const std::vector<std::size_t> body_counts = {10, 100, 500};
    constexpr int kSteps = 20;
    constexpr int kRepeats = 2;

    std::cout << "OrbitSimLite benchmark: integrator runtime comparison\n";
    std::cout << "Each configuration runs " << kSteps
              << " simulation steps and averages " << kRepeats << " repeats.\n\n";

    std::cout << std::left
              << std::setw(10) << "Bodies"
              << std::setw(12) << "Method"
              << std::setw(10) << "Steps"
              << std::setw(10) << "Repeats"
              << std::setw(16) << "Avg total ms"
              << std::setw(16) << "Ms / step"
              << '\n';
    std::cout << std::string(74, '-') << '\n';
    std::cout.flush();

    for (std::size_t count : body_counts) {
        print_result_row(run_benchmark(count, Integrator::Euler, kSteps, kRepeats));
        print_result_row(run_benchmark(count, Integrator::RK4, kSteps, kRepeats));
    }

    return 0;
}
