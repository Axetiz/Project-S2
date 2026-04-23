// OrbitSimLite - Renderer drawing and UI implementation
#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace orbitsimlite {

namespace {

// Reused to build lightweight primitive arrays without repeating boilerplate.
constexpr double kEarthYearSeconds = 365.25 * 24.0 * 3600.0;

sf::Vertex make_vertex(const sf::Vector2f& position, const sf::Color& color) {
    sf::Vertex vertex;
    vertex.position = position;
    vertex.color = color;
    return vertex;
}

} // namespace
std::string Renderer::field_mode_name() const {
    switch (field_mode_) {
    case FieldMode::None:
        return "None";
    case FieldMode::Arrows:
        return "Arrows";
    case FieldMode::Grid:
        return "Curved grid";
    }
    return "Unknown";
}

void Renderer::draw_field_overlay(sf::RenderWindow& window, const Simulator& sim) const {
    // The field overlay acts like a mode switch: exactly one field view is
    // active at a time so the simulation canvas stays readable.
    if (field_mode_ == FieldMode::None) {
        return;
    }

    if (field_mode_ == FieldMode::Arrows) {
        draw_arrow_field_overlay(window, sim);
        return;
    }

    draw_grid_field_overlay(window, sim);
}

void Renderer::draw_arrow_field_overlay(sf::RenderWindow& window, const Simulator& sim) const {
    const auto& bodies = sim.get_bodies();
    if (bodies.empty()) {
        return;
    }

    // Sample the current gravitational field on a coarse screen grid. This is
    // a visual aid only, so readability matters more than physical density.
    const float sim_width = static_cast<float>(width_) - side_panel_width_;
    const float sim_height = static_cast<float>(height_);
    const float spacing = 56.0f;
    const float margin = 24.0f;

    for (float sy = margin; sy < sim_height - margin; sy += spacing) {
        for (float sx = margin; sx < sim_width - margin; sx += spacing) {
            const Vec2 world = screen_to_world(sf::Vector2f{sx, sy});
            Body probe(1.0, world, Vec2{0.0, 0.0}, 1.0, 0xFFFFFF);
            const Vec2 acc = Physics::acceleration(probe, bodies, sim.get_gravity());
            const double mag = acc.length();
            if (mag <= 0.0) {
                continue;
            }

            const Vec2 dir = acc.normalized();
            const double intensity = std::min(1.0, std::log10(1.0 + mag) / 6.5);
            const float len = static_cast<float>(14.0 + 26.0 * intensity);

            const sf::Vector2f start{sx, sy};
            const sf::Vector2f end{
                sx + static_cast<float>(dir.x * len),
                sy - static_cast<float>(dir.y * len),
            };

            const sf::Color color(
                static_cast<std::uint8_t>(210 + 45 * intensity),
                static_cast<std::uint8_t>(220 + 35 * intensity),
                static_cast<std::uint8_t>(255),
                static_cast<std::uint8_t>(170 + 80 * intensity));

            sf::Vertex line[] = {
                make_vertex(start, color),
                make_vertex(end, color),
            };
#if SFML_VERSION_MAJOR >= 3
            window.draw(line, 2, sf::PrimitiveType::Lines);
#else
            window.draw(line, 2, sf::Lines);
#endif

            const sf::Vector2f delta = end - start;
            const float arrow_scale = 0.22f;
            const sf::Vector2f left{
                end.x - arrow_scale * (delta.x + delta.y),
                end.y - arrow_scale * (delta.y - delta.x),
            };
            const sf::Vector2f right{
                end.x - arrow_scale * (delta.x - delta.y),
                end.y - arrow_scale * (delta.y + delta.x),
            };
            sf::Vertex arrow_left[] = {
                make_vertex(end, color),
                make_vertex(left, color),
            };
            sf::Vertex arrow_right[] = {
                make_vertex(end, color),
                make_vertex(right, color),
            };
#if SFML_VERSION_MAJOR >= 3
            window.draw(arrow_left, 2, sf::PrimitiveType::Lines);
            window.draw(arrow_right, 2, sf::PrimitiveType::Lines);
#else
            window.draw(arrow_left, 2, sf::Lines);
            window.draw(arrow_right, 2, sf::Lines);
#endif
        }
    }
}

void Renderer::draw_grid_field_overlay(sf::RenderWindow& window, const Simulator& sim) const {
    const auto& bodies = sim.get_bodies();
    if (bodies.empty()) {
        return;
    }

    const float sim_width = static_cast<float>(width_) - side_panel_width_;
    const float sim_height = static_cast<float>(height_);
    const float grid_spacing_px = 30.6667f;
    const float sample_step_px = 6.0f;
    const float margin = 14.0f;

    // Estimate a scene-wide scale so the mesh adapts to both SI-unit demos and
    // dimensionless figure-eight scenes without manual tuning per preset.
    double max_mass = 0.0;
    for (const auto& b : bodies) {
        max_mass = std::max(max_mass, b.mass);
    }
    max_mass = std::max(max_mass, 1.0);

    double scene_radius = 0.0;
    for (const auto& b : bodies) {
        scene_radius = std::max(scene_radius, b.pos.length());
    }
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            scene_radius = std::max(scene_radius, Vec2::distance(bodies[i].pos, bodies[j].pos));
        }
    }

    const Vec2 top_left_world = screen_to_world(sf::Vector2f{margin, margin});
    const Vec2 bottom_right_world = screen_to_world(sf::Vector2f{sim_width - margin, sim_height - margin});
    scene_radius = std::max(scene_radius, 0.5 * Vec2::distance(top_left_world, bottom_right_world));
    scene_radius = std::max(scene_radius, 1e-3);
    const double field_softening = scene_radius * 0.08;
    const double max_local_displacement = scene_radius * 0.030;
    const double displacement_response = 0.60;

    // The curved-grid field is a visualization layer only. We sample a
    // softened Newtonian field and use its direction to warp a square mesh
    // without introducing unstable singular spikes near body centers.
    const auto softened_field = [&](const Vec2& world) {
        Vec2 field {};
        for (const auto& b : bodies) {
            const Vec2 delta = b.pos - world;
            const double visual_radius =
                pixels_to_meters(std::max(8.0, b.radius + 4.0), scale_ * zoom_);
            const double soften = std::max(field_softening, visual_radius);
            const double dist2 = delta.length_squared() + soften * soften;
            const double inv_dist = 1.0 / std::sqrt(dist2);
            const double inv_dist3 = inv_dist * inv_dist * inv_dist;
            field += (sim.get_gravity() * b.mass) * (delta * inv_dist3);
        }
        return field;
    };

    // Use a stable scene-scale reference instead of per-frame probe maxima.
    // That keeps the whole mesh from re-normalizing itself every time bodies
    // move a little, which is what caused the visible shaking.
    const double reference_distance = std::max(scene_radius * 0.22, field_softening);
    const double field_scale =
        std::max(1e-12, sim.get_gravity() * max_mass / (reference_distance * reference_distance));

    const auto warp_world_point = [&](const Vec2& world) {
        const Vec2 field = softened_field(world);
        const double magnitude = field.length();
        if (magnitude <= 1e-12) {
            return world;
        }

        const Vec2 dir = field / magnitude;
        const double normalized_strength =
            1.0 - std::exp(-displacement_response * std::sqrt(magnitude / field_scale));
        const double displacement = max_local_displacement * normalized_strength;
        return world + dir * displacement;
    };

    // Draw one warped grid line by sampling it in small screen-space segments.
    auto draw_polyline = [&](bool vertical, float fixed) {
        std::vector<sf::Vertex> verts;
        if (vertical) {
            for (float sy = margin; sy <= sim_height - margin; sy += sample_step_px) {
                const Vec2 world = screen_to_world(sf::Vector2f{fixed, sy});
                const sf::Vector2f screen = world_to_screen(warp_world_point(world));
                verts.push_back(make_vertex(screen, sf::Color(245, 248, 255, 210)));
            }
        } else {
            for (float sx = margin; sx <= sim_width - margin; sx += sample_step_px) {
                const Vec2 world = screen_to_world(sf::Vector2f{sx, fixed});
                const sf::Vector2f screen = world_to_screen(warp_world_point(world));
                verts.push_back(make_vertex(screen, sf::Color(245, 248, 255, 210)));
            }
        }

#if SFML_VERSION_MAJOR >= 3
        window.draw(verts.data(), verts.size(), sf::PrimitiveType::LineStrip);
#else
        window.draw(verts.data(), verts.size(), sf::LineStrip);
#endif
    };

    for (float sx = margin; sx <= sim_width - margin; sx += grid_spacing_px) {
        draw_polyline(true, sx);
    }

    for (float sy = margin; sy <= sim_height - margin; sy += grid_spacing_px) {
        draw_polyline(false, sy);
    }

    for (float sx = margin; sx <= sim_width - margin; sx += grid_spacing_px) {
        for (float sy = margin; sy <= sim_height - margin; sy += grid_spacing_px) {
            const Vec2 world = screen_to_world(sf::Vector2f{sx, sy});
            const sf::Vector2f screen = world_to_screen(warp_world_point(world));
            sf::CircleShape node(1.2f);
            node.setFillColor(sf::Color(255, 255, 255, 225));
            node.setPosition(sf::Vector2f{screen.x - 1.2f, screen.y - 1.2f});
            window.draw(node);
        }
    }
}

void Renderer::draw_arrow(sf::RenderWindow& window,
                          const sf::Vector2f& start,
                          const Vec2& world_vec,
                          const sf::Color& color,
                          float scale,
                          float max_length) const {
    // Logarithmic scaling keeps very large vectors visible without letting
    // extreme values dominate the whole scene.
    const double mag = world_vec.length();
    if (mag <= 0.0) {
        return;
    }

    const Vec2 dir = world_vec.normalized();
    const float length = std::min(max_length, std::max(10.0f, scale * static_cast<float>(std::log10(1.0 + mag))));
    const sf::Vector2f end{
        start.x + static_cast<float>(dir.x * length),
        start.y - static_cast<float>(dir.y * length),
    };

    sf::Vertex shaft[] = {
        make_vertex(start, color),
        make_vertex(end, color),
    };
#if SFML_VERSION_MAJOR >= 3
    window.draw(shaft, 2, sf::PrimitiveType::Lines);
#else
    window.draw(shaft, 2, sf::Lines);
#endif

    const sf::Vector2f delta = end - start;
    const float arrow_scale = 0.24f;
    sf::Vertex left[] = {
        make_vertex(end, color),
        make_vertex(sf::Vector2f{
            end.x - arrow_scale * (delta.x + delta.y),
            end.y - arrow_scale * (delta.y - delta.x)}, color),
    };
    sf::Vertex right[] = {
        make_vertex(end, color),
        make_vertex(sf::Vector2f{
            end.x - arrow_scale * (delta.x - delta.y),
            end.y - arrow_scale * (delta.y + delta.x)}, color),
    };
#if SFML_VERSION_MAJOR >= 3
    window.draw(left, 2, sf::PrimitiveType::Lines);
    window.draw(right, 2, sf::PrimitiveType::Lines);
#else
    window.draw(left, 2, sf::Lines);
    window.draw(right, 2, sf::Lines);
#endif
}

void Renderer::draw_body_trails(sf::RenderWindow& window, const Simulator& sim) {
    const auto& bodies = sim.get_bodies();
    if (trails_.size() != bodies.size()) {
        rebuild_trails(bodies.size());
    }

    // Trails are stored in world coordinates and only projected during draw.
    // That keeps them stable while the camera moves around the scene.
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const auto& body = bodies[i];
        auto& trail = trails_[i];
        trail.push_back(body.pos);
        if (trail.size() > max_trail_) {
            trail.pop_front();
        }

        std::uint8_t r, g, b;
        unpack_rgb(body.color, r, g, b);
        const sf::Color c(r, g, b, 180);

        if (trail.size() < 2) {
            continue;
        }

#if SFML_VERSION_MAJOR >= 3
        sf::VertexArray lines(sf::PrimitiveType::LineStrip, trail.size());
#else
        sf::VertexArray lines(sf::LineStrip, trail.size());
#endif
        std::size_t idx = 0;
        for (const auto& point : trail) {
            lines[idx].position = world_to_screen(point);
            const float t = static_cast<float>(idx) / static_cast<float>(trail.size());
            lines[idx].color = sf::Color(c.r, c.g, c.b, static_cast<std::uint8_t>(50 + 200 * t));
            ++idx;
        }
        window.draw(lines);
    }
}

void Renderer::draw_bodies(sf::RenderWindow& window, const Simulator& sim) const {
    const auto& bodies = sim.get_bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const auto& body = bodies[i];
        sf::CircleShape circle(static_cast<float>(body.radius));
        std::uint8_t r, g, b;
        unpack_rgb(body.color, r, g, b);
        circle.setFillColor(sf::Color(r, g, b));
        const sf::Vector2f p = world_to_screen(body.pos);
        circle.setPosition(sf::Vector2f{p.x - circle.getRadius(), p.y - circle.getRadius()});
        window.draw(circle);
        if (static_cast<int>(i) == selected_body_idx_) {
            draw_body_highlight(window, body);
        }
    }
}

void Renderer::draw_body_vectors(sf::RenderWindow& window, const Simulator& sim) const {
    if (!show_vectors_) {
        return;
    }

    // Velocity and acceleration use different colors so users can compare
    // kinematics and forces at a glance.
    const auto& bodies = sim.get_bodies();
    for (const auto& b : bodies) {
        const sf::Vector2f start = world_to_screen(b.pos);
        draw_arrow(window, start, b.vel, sf::Color(120, 240, 255, 230), 7.5f, 72.0f);
        draw_arrow(window, start, b.acc, sf::Color(255, 170, 120, 220), 14.0f, 50.0f);
    }
}

void Renderer::draw_prediction_path(sf::RenderWindow& window, const Simulator& sim) const {
    const auto& bodies = sim.get_bodies();
    if (!show_prediction_ || selected_body_idx_ < 0 ||
        selected_body_idx_ >= static_cast<int>(bodies.size())) {
        return;
    }

    // Run a temporary copy of the simulation forward so the preview reflects
    // the currently selected integrator, timestep, and substep settings.
    Simulator preview(sim.get_gravity(), sim.get_dt(), sim.get_integrator());
    preview.set_substeps(sim.get_substeps());
    preview.set_bodies(bodies);

    const std::size_t target_idx = static_cast<std::size_t>(selected_body_idx_);
    const std::size_t preview_steps = 180;
    std::vector<sf::Vertex> verts;
    verts.reserve(preview_steps + 1);

    const Body& selected = bodies[target_idx];
    std::uint8_t r, g, b;
    unpack_rgb(selected.color, r, g, b);
    const sf::Color base_color(r, g, b, 180);

    verts.push_back(make_vertex(world_to_screen(selected.pos), base_color));
    for (std::size_t i = 0; i < preview_steps; ++i) {
        preview.step();
        const auto& preview_bodies = preview.get_bodies();
        if (target_idx >= preview_bodies.size()) {
            break;
        }

        const float t = static_cast<float>(i + 1) / static_cast<float>(preview_steps);
        const sf::Color c(
            static_cast<std::uint8_t>(std::min(255, static_cast<int>(r) + 20)),
            static_cast<std::uint8_t>(std::min(255, static_cast<int>(g) + 20)),
            static_cast<std::uint8_t>(std::min(255, static_cast<int>(b) + 20)),
            static_cast<std::uint8_t>(70 + 150 * (1.0f - t)));
        verts.push_back(make_vertex(world_to_screen(preview_bodies[target_idx].pos), c));
    }

    if (verts.size() < 2) {
        return;
    }

#if SFML_VERSION_MAJOR >= 3
    window.draw(verts.data(), verts.size(), sf::PrimitiveType::LineStrip);
#else
    window.draw(verts.data(), verts.size(), sf::LineStrip);
#endif
}

void Renderer::draw_body_highlight(sf::RenderWindow& window, const Body& body) const {
    sf::CircleShape ring(static_cast<float>(body.radius) + 5.0f);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(2.0f);
    ring.setOutlineColor(sf::Color(255, 255, 255, 180));
    const sf::Vector2f p = world_to_screen(body.pos);
    ring.setPosition(sf::Vector2f{p.x - ring.getRadius(), p.y - ring.getRadius()});
    window.draw(ring);
}

void Renderer::draw_editor_cursor(sf::RenderWindow& window, const Vec2& world_point) const {
    const sf::Vector2f p = world_to_screen(world_point);
    const sf::Color c(255, 255, 255, 110);
    sf::Vertex h[] = {
        make_vertex(sf::Vector2f{p.x - 8.0f, p.y}, c),
        make_vertex(sf::Vector2f{p.x + 8.0f, p.y}, c),
    };
    sf::Vertex v[] = {
        make_vertex(sf::Vector2f{p.x, p.y - 8.0f}, c),
        make_vertex(sf::Vector2f{p.x, p.y + 8.0f}, c),
    };
#if SFML_VERSION_MAJOR >= 3
    window.draw(h, 2, sf::PrimitiveType::Lines);
    window.draw(v, 2, sf::PrimitiveType::Lines);
#else
    window.draw(h, 2, sf::Lines);
    window.draw(v, 2, sf::Lines);
#endif
}

void Renderer::draw_scene_io_overlay(sf::RenderWindow& window) const {
    if (scene_io_mode_ == SceneIoMode::None || !font_loaded_) {
        return;
    }

    // Render scene import/export as a centered modal so it never competes with
    // the side panel for space or with the simulation canvas for visibility.
    sf::RectangleShape veil(sf::Vector2f{
        static_cast<float>(width_) - side_panel_width_,
        static_cast<float>(height_),
    });
    veil.setFillColor(sf::Color(0, 0, 0, 120));
    window.draw(veil);

    const float box_width = 520.0f;
    const float box_height = scene_io_mode_ == SceneIoMode::Save ? 180.0f : 300.0f;
    const float sim_width = static_cast<float>(width_) - side_panel_width_;
    const sf::Vector2f box_pos{
        (sim_width - box_width) * 0.5f,
        (static_cast<float>(height_) - box_height) * 0.5f,
    };

    sf::RectangleShape box(sf::Vector2f{box_width, box_height});
    box.setPosition(box_pos);
    box.setFillColor(sf::Color(18, 22, 32, 245));
    box.setOutlineThickness(2.0f);
    box.setOutlineColor(sf::Color(120, 155, 200, 210));
    window.draw(box);

    const auto add_text = [&](const std::string& str, float x, float y, unsigned size, sf::Color color) {
#if SFML_VERSION_MAJOR >= 3
        sf::Text text(ui_font_, str, size);
#else
        sf::Text text(str, ui_font_, size);
#endif
        text.setFillColor(color);
        text.setPosition(sf::Vector2f{x, y});
        window.draw(text);
    };

    float y = box_pos.y + 18.0f;
    const float x = box_pos.x + 18.0f;
    if (scene_io_mode_ == SceneIoMode::Save) {
        add_text("Export Scene", x, y, 22, sf::Color(240, 245, 255));
        y += 34.0f;
        add_text("File will be saved to Exports/<name>.json", x, y, 15, sf::Color(180, 205, 230));
        y += 32.0f;

        sf::RectangleShape input_box(sf::Vector2f{box_width - 36.0f, 42.0f});
        input_box.setPosition(sf::Vector2f{x, y});
        input_box.setFillColor(sf::Color(10, 14, 24, 255));
        input_box.setOutlineThickness(1.0f);
        input_box.setOutlineColor(sf::Color(95, 120, 155, 220));
        window.draw(input_box);
        add_text(scene_io_input_.empty() ? "Enter scene name..." : scene_io_input_,
                 x + 10.0f, y + 8.0f, 18,
                 scene_io_input_.empty() ? sf::Color(130, 145, 165) : sf::Color(245, 247, 252));
        y += 58.0f;
        if (scene_io_confirm_overwrite_) {
            add_text("This file already exists in Exports/.", x, y, 15, sf::Color(255, 214, 140));
            y += 22.0f;
            add_text("Press Y to overwrite | Edit name | Esc cancel", x, y, 15, sf::Color(180, 205, 230));
        } else {
            add_text("Enter save | Backspace edit | Esc cancel", x, y, 15, sf::Color(180, 205, 230));
        }
    } else {
        add_text("Import Scene", x, y, 22, sf::Color(240, 245, 255));
        y += 34.0f;
        add_text("Load saved scene from Exports/", x, y, 15, sf::Color(180, 205, 230));
        y += 24.0f;

        if (saved_scene_files_.empty()) {
            add_text("No saved scene files found.", x, y + 12.0f, 18, sf::Color(220, 225, 240));
        } else {
            for (std::size_t i = 0; i < saved_scene_files_.size() && i < 8; ++i) {
                const bool selected = static_cast<int>(i) == selected_saved_scene_idx_;
                sf::RectangleShape row(sf::Vector2f{box_width - 36.0f, 28.0f});
                row.setPosition(sf::Vector2f{x, y + 2.0f});
                row.setFillColor(selected ? sf::Color(50, 76, 108, 220) : sf::Color(12, 16, 28, 180));
                row.setOutlineThickness(selected ? 1.0f : 0.0f);
                row.setOutlineColor(sf::Color(140, 180, 225, 200));
                window.draw(row);
                add_text(saved_scene_files_[i], x + 8.0f, y + 4.0f, 16, sf::Color(235, 240, 248));
                y += 32.0f;
            }
        }
        y = box_pos.y + box_height - 34.0f;
        if (scene_io_confirm_delete_ && !saved_scene_files_.empty()) {
            add_text("Delete selected file from Exports/?", x, y - 22.0f, 15, sf::Color(255, 214, 140));
            add_text("Y confirm delete | Esc cancel | Up/Down keep browsing", x, y, 15, sf::Color(180, 205, 230));
        } else {
            add_text("Up/Down select | Enter load | Delete remove | Esc cancel", x, y, 15, sf::Color(180, 205, 230));
        }
    }
}

void Renderer::draw_side_panel(sf::RenderWindow& window, const Simulator& sim, const std::string& preset_name) const {
    sf::RectangleShape panel(sf::Vector2f{side_panel_width_, static_cast<float>(height_)});
    panel.setPosition(sf::Vector2f{static_cast<float>(width_) - side_panel_width_, 0.0f});
    panel.setFillColor(sf::Color(18, 22, 32, 240));
    panel.setOutlineThickness(1.0f);
    panel.setOutlineColor(sf::Color(70, 80, 100, 180));
    window.draw(panel);

    if (!font_loaded_) {
        return;
    }

    const auto add_text = [&](const std::string& str, float x, float y, unsigned size, sf::Color color) {
#if SFML_VERSION_MAJOR >= 3
        sf::Text text(ui_font_, str, size);
#else
        sf::Text text(str, ui_font_, size);
#endif
        text.setFillColor(color);
        text.setPosition(sf::Vector2f{x, y});
        window.draw(text);
    };

    // The side panel is intentionally dense but vertically structured: system
    // status first, selected-body data second, controls last.
    const float left = static_cast<float>(width_) - side_panel_width_ + 16.0f;
    float y = 14.0f;

    add_text("OrbitSimLite 2.0", left, y, 22, sf::Color(240, 245, 255));
    y += 30.0f;
    add_text("Preset: " + preset_name, left, y, 16, sf::Color(170, 210, 255));
    y += 24.0f;
    add_text("Bodies: " + std::to_string(sim.get_bodies().size()), left, y, 15, sf::Color(220, 225, 240));
    y += 20.0f;
    add_text("Collision: " + collision_mode_name(), left, y, 15, sf::Color(220, 225, 240));
    y += 20.0f;
    add_text("Field: " + field_mode_name(), left, y, 15, sf::Color(220, 225, 240));
    y += 20.0f;
    add_text("Integrator: " + integrator_name(sim.get_integrator()), left, y, 15, sf::Color(220, 225, 240));
    y += 20.0f;
    add_text("Time scale: x" + format_scientific(time_scale_, 2), left, y, 15, sf::Color(220, 225, 240));
    y += 20.0f;
    add_text(paused_ ? "Status: paused" : "Status: running", left, y, 15, paused_ ? sf::Color(255, 210, 120) : sf::Color(150, 235, 180));
    y += 20.0f;
    add_text("Sim years: " + format_scientific(sim.get_time() / kEarthYearSeconds, 3), left, y, 15, sf::Color(220, 225, 240));
    y += 28.0f;

    add_text("Selected Body", left, y, 18, sf::Color(240, 245, 255));
    y += 24.0f;

    const auto& bodies = sim.get_bodies();
    if (selected_body_idx_ >= 0 && selected_body_idx_ < static_cast<int>(bodies.size())) {
        const Body& body = bodies[static_cast<std::size_t>(selected_body_idx_)];
        add_text("Name: " + (body.name.empty() ? "(unnamed)" : body.name), left, y, 14, sf::Color(220, 225, 240));
        y += 18.0f;
        add_text("Mass: " + format_scientific(body.mass), left, y, 14, sf::Color(220, 225, 240));
        y += 18.0f;
        add_text("Radius: " + format_scientific(body.radius, 2), left, y, 14, sf::Color(220, 225, 240));
        y += 18.0f;
        add_text("Pos: (" + format_scientific(body.pos.x, 2) + ", " + format_scientific(body.pos.y, 2) + ")", left, y, 14, sf::Color(220, 225, 240));
        y += 18.0f;
        add_text("Vel: (" + format_scientific(body.vel.x, 2) + ", " + format_scientific(body.vel.y, 2) + ")", left, y, 14, sf::Color(220, 225, 240));
        y += 18.0f;
        add_text("Acc: (" + format_scientific(body.acc.x, 2) + ", " + format_scientific(body.acc.y, 2) + ")", left, y, 14, sf::Color(220, 225, 240));
        y += 18.0f;
        add_text(body.is_star ? "Type: star" : (body.is_satellite ? "Type: satellite" : "Type: planet/body"), left, y, 14, sf::Color(220, 225, 240));
        y += 24.0f;
    } else {
        add_text("Click a body to inspect it.", left, y, 14, sf::Color(220, 225, 240));
        y += 24.0f;
    }

    add_text("Controls", left, y, 18, sf::Color(240, 245, 255));
    y += 24.0f;
    add_text("Space pause/resume", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text(". single-step while paused", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("+ / - time scale", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("R reset current preset", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("F cycle field mode", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("I cycle integrator", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("V toggle vector arrows", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("P toggle prediction", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("M cycle collision mode", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("K export scene", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("L import scene", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Left click select body", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Right drag pan view", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("N add body at cursor", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Delete remove selected", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Arrows move body", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("WASD edit velocity", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Q/E mass, Z/X radius", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("T toggle star, Y satellite", left, y, 14, sf::Color(220, 225, 240));
    if (!scene_io_message_.empty()) {
        y += 28.0f;
        add_text("Scene I/O", left, y, 18, sf::Color(240, 245, 255));
        y += 22.0f;
        add_text(scene_io_message_, left, y, 14, sf::Color(170, 215, 255));
    }
}


} // namespace orbitsimlite
