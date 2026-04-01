// OrbitSimLite - Simulator implementation
#include "simulator.hpp"

namespace orbitsimlite {

Simulator::Simulator(double G, double dt, Integrator integrator)
    : G_(G), dt_(dt), integrator_(integrator), substeps_(1), time_(0.0) {}

void Simulator::add_body(const Body& b) { bodies_.push_back(b); }

void Simulator::set_bodies(const std::vector<Body>& bs) { bodies_ = bs; }

void Simulator::clear() { bodies_.clear(); }

void Simulator::set_dt(double dt__) { dt_ = dt__; }
double Simulator::get_dt() const { return dt_; }

void Simulator::set_integrator(Integrator i) { integrator_ = i; }
Integrator Simulator::get_integrator() const { return integrator_; }

void Simulator::set_gravity(double G__) { G_ = G__; }
double Simulator::get_gravity() const { return G_; }

void Simulator::step() {
    if (bodies_.empty()) return;

    const int n = (substeps_ > 0) ? substeps_ : 1;
    const double h = dt_ / static_cast<double>(n);

    for (int s = 0; s < n; ++s) {
        if (integrator_ == Integrator::Euler) {
            // Compute accelerations at current positions
            std::vector<Vec2> accs(bodies_.size());
            for (std::size_t i = 0; i < bodies_.size(); ++i) {
                accs[i] = Physics::acceleration(bodies_[i], bodies_, G_);
            }
            // Update bodies
            for (std::size_t i = 0; i < bodies_.size(); ++i) {
                Physics::step_euler(bodies_[i], accs[i], h);
            }
        } else { // RK4
            // Compute new states into a copy to avoid order dependence
            std::vector<Body> next = bodies_;
            for (std::size_t i = 0; i < bodies_.size(); ++i) {
                // For RK4, exclude self from the accelerations used in intermediate stages
                std::vector<Body> others;
                others.reserve(bodies_.size() - 1);
                for (std::size_t j = 0; j < bodies_.size(); ++j) {
                    if (j == i) continue;
                    others.push_back(bodies_[j]);
                }
                Physics::step_rk4(next[i], others, G_, h);
            }
            bodies_ = std::move(next);
        }
    }

    // Advance simulation time by one full step
    time_ += dt_;
}

const std::vector<Body>& Simulator::get_bodies() const { return bodies_; }
std::vector<Body>& Simulator::access_bodies() { return bodies_; }

void Simulator::set_substeps(int n) { substeps_ = (n > 0) ? n : 1; }
int Simulator::get_substeps() const { return substeps_; }

double Simulator::get_time() const { return time_; }
void Simulator::reset_time() { time_ = 0.0; }

} // namespace orbitsimlite
