// OrbitSimLite - Renderer collision workflow
#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace orbitsimlite {

std::string Renderer::collision_mode_name() const {
    switch (collision_mode_) {
    case CollisionMode::PromptRemoveSmaller:
        return "Prompt remove";
    case CollisionMode::RemoveSmaller:
        return "Auto remove";
    case CollisionMode::Merge:
        return "Merge";
    case CollisionMode::Ignore:
        return "Ignore";
    }
    return "Unknown";
}

void Renderer::handle_collision_resolution(Simulator& sim, std::size_t idx_a, std::size_t idx_b) {
    auto& bodies = sim.access_bodies();
    if (idx_a >= bodies.size() || idx_b >= bodies.size() || idx_a == idx_b) {
        return;
    }

    // Normalize the pair into "keep" and "remove" roles once, then let the
    // chosen collision mode decide what to do with that ordering.
    std::size_t remove_idx = idx_a;
    std::size_t keep_idx = idx_b;
    if (bodies[idx_b].mass < bodies[idx_a].mass) {
        remove_idx = idx_b;
        keep_idx = idx_a;
    }

    if (collision_mode_ == CollisionMode::Ignore) {
        return;
    }

    if (collision_mode_ == CollisionMode::RemoveSmaller) {
        bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(remove_idx));
        if (remove_idx < trails_.size()) {
            trails_.erase(trails_.begin() + static_cast<std::ptrdiff_t>(remove_idx));
        }
        return;
    }

    if (collision_mode_ == CollisionMode::Merge) {
        Body merged = bodies[keep_idx];
        const Body& other = bodies[remove_idx];
        const double total_mass = merged.mass + other.mass;
        merged.pos = (merged.pos * merged.mass + other.pos * other.mass) / total_mass;
        merged.vel = (merged.vel * merged.mass + other.vel * other.mass) / total_mass;
        merged.mass = total_mass;
        merged.radius = std::sqrt(merged.radius * merged.radius + other.radius * other.radius);
        merged.is_star = merged.is_star || other.is_star;
        merged.is_satellite = merged.is_satellite && other.is_satellite;
        merged.name = merged.name + "+" + other.name;
        bodies[keep_idx] = merged;
        bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(remove_idx));
        if (remove_idx < trails_.size()) {
            trails_.erase(trails_.begin() + static_cast<std::ptrdiff_t>(remove_idx));
        }
    }
}

void Renderer::remove_bodies_by_index(Simulator& sim, const std::vector<std::size_t>& to_remove) {
    if (to_remove.empty()) {
        return;
    }

    auto& bodies = sim.access_bodies();
    std::vector<std::size_t> unique = to_remove;
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());

    for (auto it = unique.rbegin(); it != unique.rend(); ++it) {
        const std::size_t idx = *it;
        if (idx < bodies.size()) {
            bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(idx));
        }
        if (idx < trails_.size()) {
            trails_.erase(trails_.begin() + static_cast<std::ptrdiff_t>(idx));
        }
    }
}

void Renderer::confirm_collision_prompt(Simulator& sim) {
    // Prompt mode stores the candidate indices in advance, so confirmation can
    // stay trivial and avoid recomputing the collision pair later.
    auto& bodies = sim.access_bodies();
    if (collision_idx_remove_ < bodies.size()) {
        bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(collision_idx_remove_));
    }
    if (collision_idx_remove_ < trails_.size()) {
        trails_.erase(trails_.begin() + static_cast<std::ptrdiff_t>(collision_idx_remove_));
    }
    collision_active_ = false;
    paused_ = false;
    std::cout << "Collision resolved: removed smaller body.\n";
    sync_selected_index(sim);
}

void Renderer::resolve_scene_collisions(Simulator& sim) {
    const auto& bodies = sim.get_bodies();
    std::vector<std::size_t> to_remove;
    bool scene_changed = false;
    if (!collision_active_) {
        // Scan the visible bodies for overlaps after the simulation step. The
        // renderer uses screen-space radii here because collisions are meant
        // to match what the user sees in the explorer.
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                const auto& a = bodies[i];
                const auto& b = bodies[j];
                const sf::Vector2f pa = world_to_screen(a.pos);
                const sf::Vector2f pb = world_to_screen(b.pos);

                const float dx = pa.x - pb.x;
                const float dy = pa.y - pb.y;
                const float dist2 = dx * dx + dy * dy;
                const float rsum = static_cast<float>(a.radius + b.radius);

                if (dist2 <= rsum * rsum) {
                    if (a.is_star ^ b.is_star) {
                        to_remove.push_back(a.is_star ? j : i);
                        continue;
                    }

                    if (a.is_satellite || b.is_satellite) {
                        continue;
                    }

                    if (collision_mode_ == CollisionMode::RemoveSmaller ||
                        collision_mode_ == CollisionMode::Merge ||
                        collision_mode_ == CollisionMode::Ignore) {
                        handle_collision_resolution(sim, i, j);
                        sync_selected_index(sim);
                        scene_changed = true;
                        break;
                    }

                    collision_active_ = true;
                    paused_ = true;
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
            if (collision_active_ || scene_changed) {
                break;
            }
        }
    }

    if (!to_remove.empty()) {
        remove_bodies_by_index(sim, to_remove);
        sync_selected_index(sim);
    }
}

} // namespace orbitsimlite
