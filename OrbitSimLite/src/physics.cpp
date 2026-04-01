// OrbitSimLite - Physics implementation
//
// The routines in this file implement:
//  - Newtonian gravitational acceleration between point masses
//  - a symplectic Euler integrator
//  - an educational Runge–Kutta 4 (RK4) step
//
// All calculations are performed in double precision using SI units
// (metres, kilograms, seconds).
#include "physics.hpp"

#include <algorithm>

namespace orbitsimlite {

// Softening term to avoid numerical singularities when two positions become
// extremely close. This is intentionally small compared to the astronomical
// distances used in the demos but prevents division-by-zero.
static constexpr double kEps2 = 1e-9;

Vec2 Physics::acceleration(const Body& target, const std::vector<Body>& others, double G) {
    Vec2 acc{0.0, 0.0};
    for (const auto& o : others) {
        // Skip exact same position to avoid singularity; also covers the case
        // where 'target' might accidentally be present in 'others'.
        Vec2 r = o.pos - target.pos;
        double dist2 = r.length_squared();
        if (dist2 <= kEps2) continue;
        double invDist = 1.0 / std::sqrt(dist2);
        double invDist3 = invDist * invDist * invDist;
        // a = G * m / r^2 * r_hat = G * m * r / |r|^3
        acc += (G * o.mass) * (r * invDist3);
    }
    return acc;
}

void Physics::step_euler(Body& body, const Vec2& acc, double dt) {
    body.acc = acc;
    // Symplectic Euler: update velocity using current acceleration, then
    // update position using the new velocity.
    body.vel += body.acc * dt;
    body.pos += body.vel * dt;
}

void Physics::step_rk4(Body& body, const std::vector<Body>& others, double G, double dt) {
    // Educational RK4 using 'others' at their current positions.
    //
    // We treat the velocity v and position x of a single body as the state
    // and integrate according to:
    //   x' = v
    //   v' = a(x),  where a(x) is computed from Newtonian gravity.
    //
    // For a concise derivation and stability discussion see:
    //   https://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods
    auto acc_at = [&](const Vec2& pos) {
        Body temp = body;
        temp.pos = pos;
        return Physics::acceleration(temp, others, G);
    };

    const Vec2 x0 = body.pos;
    const Vec2 v0 = body.vel;

    const Vec2 k1_v = acc_at(x0);
    const Vec2 k1_x = v0;

    const Vec2 k2_v = acc_at(x0 + 0.5 * dt * k1_x);
    const Vec2 k2_x = v0 + 0.5 * dt * k1_v;

    const Vec2 k3_v = acc_at(x0 + 0.5 * dt * k2_x);
    const Vec2 k3_x = v0 + 0.5 * dt * k2_v;

    const Vec2 k4_v = acc_at(x0 + dt * k3_x);
    const Vec2 k4_x = v0 + dt * k3_v;

    body.vel = v0 + (dt / 6.0) * (k1_v + 2.0 * k2_v + 2.0 * k3_v + k4_v);
    body.pos = x0 + (dt / 6.0) * (k1_x + 2.0 * k2_x + 2.0 * k3_x + k4_x);
    body.acc = acc_at(body.pos);
}

} // namespace orbitsimlite
