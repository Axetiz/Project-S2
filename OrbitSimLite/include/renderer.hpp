// OrbitSimLite - SFML-based renderer
//
// Responsible for visualising the current Simulator state:
//  - fixed world-to-screen mapping (metres -> pixels)
//  - drawing bodies as circles with fading trails
//  - basic interactive controls (pause, reset, collision handling)
//  - continuous export of the current state to a JSON file
#pragma once

#include <deque>
#include <vector>
#include <cstdint>
#include <string>

#include <SFML/Graphics.hpp>

#include "simulator.hpp"
#include "utils.hpp"

namespace orbitsimlite {

class Renderer {
public:
    Renderer(unsigned width = 1000, unsigned height = 800, double meters_to_pixels = 2e-9);

    // Runs the visualization loop. Blocks until window close.
    void run(Simulator& sim);

private:
    sf::Vector2f world_to_screen(const Vec2& p) const;
    void rebuild_trails(std::size_t count);
    void write_state_json(const Simulator& sim, const std::string& filename) const;

    unsigned width_;
    unsigned height_;
    double scale_; // meters to pixels
    bool paused_ {false};
    const std::size_t max_trail_ = 200;

    std::vector<std::deque<sf::Vector2f>> trails_;

    // Collision handling state
    bool collision_active_ {false};
    std::size_t collision_idx_keep_ {0};
    std::size_t collision_idx_remove_ {0};

    // JSON state output file (overwritten each frame)
    std::string state_filename_ {"bodies.json"};
};

} // namespace orbitsimlite
