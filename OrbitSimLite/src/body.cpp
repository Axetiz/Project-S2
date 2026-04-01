// OrbitSimLite - Body implementation
#include "body.hpp"

namespace orbitsimlite {

Body::Body()
    : mass(0.0), radius(1.0), pos(), vel(), acc(), color(0xFFFFFF),
      is_satellite(false), is_star(false), name() {}

Body::Body(double mass_, const Vec2& pos_, const Vec2& vel_, double radius_, std::uint32_t color_,
           bool is_satellite_, bool is_star_, const std::string& name_)
    : mass(mass_), radius(radius_), pos(pos_), vel(vel_), acc(0.0, 0.0), color(color_),
      is_satellite(is_satellite_), is_star(is_star_), name(name_) {}

} // namespace orbitsimlite
