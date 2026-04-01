// OrbitSimLite - Renderer implementation (SFML)
#include "renderer.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace orbitsimlite {

Renderer::Renderer(unsigned width, unsigned height, double meters_to_pixels)
    : width_(width), height_(height), scale_(meters_to_pixels) {}

sf::Vector2f Renderer::world_to_screen(const Vec2& p) const {
    // Place world origin at screen center, +y upwards (SFML y is downwards)
    float cx = static_cast<float>(width_ / 2.0);
    float cy = static_cast<float>(height_ / 2.0);
    float x = static_cast<float>(meters_to_pixels(p.x, scale_));
    float y = static_cast<float>(meters_to_pixels(p.y, scale_));
    return sf::Vector2f{cx + x, cy - y};
}

void Renderer::rebuild_trails(std::size_t count) {
    trails_.clear();
    trails_.resize(count);
}

void Renderer::write_state_json(const Simulator& sim, const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) {
        return;
    }

    const auto& bodies = sim.get_bodies();

    out << "{\n";
    out << "  \"bodies\": [\n";
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const auto& b = bodies[i];
        std::string name = b.name.empty() ? ("body_" + std::to_string(i)) : b.name;
        out << "    {\n";
        out << "      \"name\": \"" << name << "\",\n";
        out << "      \"mass\": " << b.mass << ",\n";
        out << "      \"radius\": " << b.radius << ",\n";
        out << "      \"color\": " << b.color << ",\n";
        out << "      \"position\": { \"x\": " << b.pos.x << ", \"y\": " << b.pos.y << " },\n";
        out << "      \"velocity\": { \"x\": " << b.vel.x << ", \"y\": " << b.vel.y << " },\n";
        out << "      \"acceleration\": { \"x\": " << b.acc.x << ", \"y\": " << b.acc.y << " }\n";
        out << "    }";
        if (i + 1 < bodies.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void Renderer::run(Simulator& sim) {
    sf::RenderWindow window(sf::VideoMode(width_, height_), "OrbitSimLite");
    window.setFramerateLimit(60);

    // Capture initial bodies for reset
    const std::vector<Body> initial_bodies = sim.get_bodies();
    rebuild_trails(initial_bodies.size());

    sf::Clock clock;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::Resized) {
                // Keep circles from becoming ovals when resizing
                sf::FloatRect visibleArea(
                    0.f, 0.f,
                    static_cast<float>(event.size.width),
                    static_cast<float>(event.size.height));
                window.setView(sf::View(visibleArea));
                width_ = event.size.width;
                height_ = event.size.height;
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    window.close();
                } else if (event.key.code == sf::Keyboard::Space) {
                    // Do not toggle pause when a collision is pending
                    if (!collision_active_) {
                        paused_ = !paused_;
                    }
                } else if (event.key.code == sf::Keyboard::R) {
                    sim.set_bodies(initial_bodies);
                    sim.reset_time();
                    rebuild_trails(initial_bodies.size());
                    collision_active_ = false;
                    paused_ = false;
                } else if (event.key.code == sf::Keyboard::C) {
                    // Resolve collision: remove smaller body and continue
                    if (collision_active_) {
                        auto& bodies_nc = sim.access_bodies();
                        if (collision_idx_remove_ < bodies_nc.size()) {
                            bodies_nc.erase(
                                bodies_nc.begin()
                                + static_cast<std::ptrdiff_t>(collision_idx_remove_));
                        }
                        if (collision_idx_remove_ < trails_.size()) {
                            trails_.erase(
                                trails_.begin()
                                + static_cast<std::ptrdiff_t>(collision_idx_remove_));
                        }
                        collision_active_ = false;
                        paused_ = false;
                        std::cout << "Collision resolved: removed smaller body.\n";
                    }
                }
            }
        }

        // Step simulation (only when not paused and no unresolved collision)
        if (!paused_ && !collision_active_) {
            sim.step();
        }

        // Write current state to JSON each frame (no history)
        write_state_json(sim, state_filename_);

        // Update window title with simulation time in Earth years
        {
            double years = sim.get_time() / (365.25 * 24.0 * 3600.0);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << years;
            window.setTitle("OrbitSimLite - t = " + oss.str() + " years");
        }

        // Draw
        window.clear(sf::Color(10, 10, 20));

        const auto& bodies = sim.get_bodies();
        if (trails_.size() != bodies.size()) {
            rebuild_trails(bodies.size());
        }

        // Collect bodies that fell into the sun this frame
        std::vector<std::size_t> to_remove;

        // Collision detection (visual touch using screen positions and radii)
        if (!collision_active_) {
            for (std::size_t i = 0; i < bodies.size(); ++i) {
                for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                    const auto& a = bodies[i];
                    const auto& b = bodies[j];

                    auto pa = world_to_screen(a.pos);
                    auto pb = world_to_screen(b.pos);

                    float dx = pa.x - pb.x;
                    float dy = pa.y - pb.y;
                    float dist2 = dx * dx + dy * dy;

                    float ra = static_cast<float>(a.radius);
                    float rb = static_cast<float>(b.radius);
                    float rsum = ra + rb;

                    if (dist2 <= rsum * rsum) {
                        // If exactly one is a star (e.g., Sun), remove the other instantly
                        bool aStar = a.is_star;
                        bool bStar = b.is_star;
                        if (aStar ^ bStar) {
                            std::size_t idx_remove = aStar ? j : i;
                            to_remove.push_back(idx_remove);
                            std::cout << "Body " << idx_remove << " collided with the Sun and was removed.\n";
                            continue;
                        }

                        // Ignore collisions between satellites and non-stars
                        if (a.is_satellite || b.is_satellite) {
                            continue;
                        }

                        // Collision detected between regular bodies: pause and let user decide
                        collision_active_ = true;
                        paused_ = true;

                        // Decide which body to remove: smaller mass
                        if (a.mass <= b.mass) {
                            collision_idx_remove_ = i;
                            collision_idx_keep_ = j;
                        } else {
                            collision_idx_remove_ = j;
                            collision_idx_keep_ = i;
                        }

                        std::cout << "Collision detected between bodies " << i
                                  << " and " << j
                                  << ". Press C to continue without the smaller body, or ESC to exit.\n";
                        break;
                    }
                }
                if (collision_active_) {
                    break;
                }
            }
        }

        // Remove any bodies that collided with the Sun this frame
        if (!to_remove.empty()) {
            auto& bodies_nc = sim.access_bodies();
            std::sort(to_remove.begin(), to_remove.end());
            to_remove.erase(std::unique(to_remove.begin(), to_remove.end()), to_remove.end());
            for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
                std::size_t idx = *it;
                if (idx < bodies_nc.size()) {
                    bodies_nc.erase(bodies_nc.begin() + static_cast<std::ptrdiff_t>(idx));
                }
                if (idx < trails_.size()) {
                    trails_.erase(trails_.begin() + static_cast<std::ptrdiff_t>(idx));
                }
            }
        }

        // Draw trails first
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const auto& b = bodies[i];
            auto screenPos = world_to_screen(b.pos);

            // Update trail
            auto& trail = trails_[i];
            trail.push_back(screenPos);
            if (trail.size() > max_trail_) trail.pop_front();

            // Convert color
            std::uint8_t r, g, bl;
            unpack_rgb(b.color, r, g, bl);
            sf::Color c(r, g, bl, 180);

            // Draw trail as line strip
            if (trail.size() >= 2) {
                sf::VertexArray lines(sf::LineStrip, trail.size());
                std::size_t idx = 0;
                for (const auto& tp : trail) {
                    lines[idx].position = tp;
                    // Fade older segments
                    float t = static_cast<float>(idx) / static_cast<float>(trail.size());
                    lines[idx].color = sf::Color(
                        c.r, c.g, c.b,
                        static_cast<sf::Uint8>(50 + 200 * t));
                    ++idx;
                }
                window.draw(lines);
            }
        }

        // Draw bodies on top
        for (const auto& b : bodies) {
            sf::CircleShape circle(static_cast<float>(b.radius));
            std::uint8_t r, g, bl;
            unpack_rgb(b.color, r, g, bl);
            circle.setFillColor(sf::Color(r, g, bl));
            auto p = world_to_screen(b.pos);
            circle.setPosition(p.x - circle.getRadius(), p.y - circle.getRadius());
            window.draw(circle);
        }

        window.display();
    }
}

} // namespace orbitsimlite
