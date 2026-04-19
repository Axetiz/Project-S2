// OrbitSimLite - Renderer implementation (SFML)
#include "renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>

namespace orbitsimlite {

namespace {

constexpr double kEarthYearSeconds = 365.25 * 24.0 * 3600.0;

sf::Vertex make_vertex(const sf::Vector2f& position, const sf::Color& color) {
    sf::Vertex vertex;
    vertex.position = position;
    vertex.color = color;
    return vertex;
}

std::uint32_t next_editor_color(std::size_t index) {
    static constexpr std::array<std::uint32_t, 8> kPalette = {
        rgb_u32(255, 120, 120),
        rgb_u32(120, 220, 255),
        rgb_u32(255, 210, 120),
        rgb_u32(170, 255, 170),
        rgb_u32(255, 160, 255),
        rgb_u32(255, 255, 120),
        rgb_u32(140, 180, 255),
        rgb_u32(255, 190, 150),
    };
    return kPalette[index % kPalette.size()];
}

} // namespace

Renderer::Renderer(unsigned width, unsigned height, double meters_to_pixels)
    : width_(width), height_(height), scale_(meters_to_pixels) {}

void Renderer::set_presets(const std::vector<ScenarioPreset>& presets) {
    presets_ = presets;
}

sf::Vector2f Renderer::world_to_screen(const Vec2& p) const {
    const float sim_width = static_cast<float>(width_) - side_panel_width_;
    const float cx = sim_width / 2.0f;
    const float cy = static_cast<float>(height_) / 2.0f;
    const Vec2 relative = p - camera_center_;
    const float x = static_cast<float>(meters_to_pixels(relative.x, scale_ * zoom_));
    const float y = static_cast<float>(meters_to_pixels(relative.y, scale_ * zoom_));
    return sf::Vector2f{cx + x, cy - y};
}

Vec2 Renderer::screen_to_world(const sf::Vector2f& p) const {
    const double sim_width = static_cast<double>(width_) - side_panel_width_;
    const double cx = sim_width / 2.0;
    const double cy = static_cast<double>(height_) / 2.0;
    return Vec2{
        pixels_to_meters(static_cast<double>(p.x) - cx, scale_ * zoom_),
        pixels_to_meters(cy - static_cast<double>(p.y), scale_ * zoom_),
    } + camera_center_;
}

void Renderer::rebuild_trails(std::size_t count) {
    trails_.clear();
    trails_.resize(count);
}

std::string Renderer::format_scientific(double value, int precision) const {
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(precision) << value;
    return oss.str();
}

void Renderer::ensure_font_loaded() {
    if (font_loaded_) {
        return;
    }

    static const std::array<const char*, 4> kFontCandidates = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    };

    for (const char* path : kFontCandidates) {
        #if SFML_VERSION_MAJOR >= 3
        if (ui_font_.openFromFile(path)) {
        #else
        if (ui_font_.loadFromFile(path)) {
        #endif
            font_loaded_ = true;
            break;
        }
    }
}

void Renderer::export_simulation_data(const Simulator& sim) {
    write_state_json(sim, state_filename_);
    record_history_frame(sim);
    write_history_json(history_filename_);
}

void Renderer::update_window_title(sf::RenderWindow& window,
                                   const Simulator& sim,
                                   const std::string& preset_name) const {
    std::ostringstream oss;
    oss << preset_name << " | t="
        << std::fixed << std::setprecision(3)
        << (sim.get_time() / kEarthYearSeconds)
        << " years | x" << time_scale_;
    window.setTitle("OrbitSimLite 2.0 - " + oss.str());
}

void Renderer::write_state_json(const Simulator& sim, const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) {
        return;
    }

    const auto& bodies = sim.get_bodies();
    out << "{\n";
    out << "  \"time_seconds\": " << sim.get_time() << ",\n";
    out << "  \"time_scale\": " << time_scale_ << ",\n";
    out << "  \"collision_mode\": \"" << collision_mode_name() << "\",\n";
    out << "  \"body_count\": " << bodies.size() << ",\n";
    out << "  \"bodies\": [\n";
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const auto& b = bodies[i];
        const std::string name = b.name.empty() ? ("body_" + std::to_string(i)) : b.name;
        out << "    {\n";
        out << "      \"name\": \"" << name << "\",\n";
        out << "      \"mass\": " << b.mass << ",\n";
        out << "      \"radius\": " << b.radius << ",\n";
        out << "      \"color\": " << b.color << ",\n";
        out << "      \"is_satellite\": " << (b.is_satellite ? "true" : "false") << ",\n";
        out << "      \"is_star\": " << (b.is_star ? "true" : "false") << ",\n";
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

void Renderer::record_history_frame(const Simulator& sim) {
    HistoryFrame frame;
    frame.time_seconds = sim.get_time();
    frame.bodies = sim.get_bodies();
    history_frames_.push_back(std::move(frame));
    while (history_frames_.size() > max_history_frames_) {
        history_frames_.pop_front();
    }
}

void Renderer::write_history_json(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) {
        return;
    }

    out << "{\n";
    out << "  \"frame_count\": " << history_frames_.size() << ",\n";
    out << "  \"frames\": [\n";
    for (std::size_t i = 0; i < history_frames_.size(); ++i) {
        const HistoryFrame& frame = history_frames_[i];
        out << "    {\n";
        out << "      \"time_seconds\": " << frame.time_seconds << ",\n";
        out << "      \"bodies\": [\n";
        for (std::size_t j = 0; j < frame.bodies.size(); ++j) {
            const Body& b = frame.bodies[j];
            const std::string name = b.name.empty() ? ("body_" + std::to_string(j)) : b.name;
            out << "        {\n";
            out << "          \"name\": \"" << name << "\",\n";
            out << "          \"mass\": " << b.mass << ",\n";
            out << "          \"radius\": " << b.radius << ",\n";
            out << "          \"position\": { \"x\": " << b.pos.x << ", \"y\": " << b.pos.y << " },\n";
            out << "          \"velocity\": { \"x\": " << b.vel.x << ", \"y\": " << b.vel.y << " },\n";
            out << "          \"acceleration\": { \"x\": " << b.acc.x << ", \"y\": " << b.acc.y << " }\n";
            out << "        }";
            if (j + 1 < frame.bodies.size()) {
                out << ",";
            }
            out << "\n";
        }
        out << "      ]\n";
        out << "    }";
        if (i + 1 < history_frames_.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

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
    add_text("1-4 switch presets", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("F cycle field mode", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("I cycle integrator", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("V toggle vector arrows", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("P toggle prediction", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("M cycle collision mode", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Left click select body", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Right drag pan view", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("N add body at cursor", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Delete remove selected", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Arrows move body", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("WASD edit velocity", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("Q/E mass, Z/X radius", left, y, 14, sf::Color(220, 225, 240)); y += 18.0f;
    add_text("T toggle star, Y satellite", left, y, 14, sf::Color(220, 225, 240));
}

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

std::string Renderer::integrator_name(Integrator integrator) const {
    switch (integrator) {
    case Integrator::Euler:
        return "Euler";
    case Integrator::RK4:
        return "RK4";
    }
    return "Unknown";
}

void Renderer::handle_collision_resolution(Simulator& sim, std::size_t idx_a, std::size_t idx_b) {
    auto& bodies = sim.access_bodies();
    if (idx_a >= bodies.size() || idx_b >= bodies.size() || idx_a == idx_b) {
        return;
    }

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
        return;
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

void Renderer::run(Simulator& sim) {
    ensure_font_loaded();

#if SFML_VERSION_MAJOR >= 3
    sf::RenderWindow window(sf::VideoMode({width_, height_}), "OrbitSimLite");
#else
    sf::RenderWindow window(sf::VideoMode(width_, height_), "OrbitSimLite");
#endif
    window.setFramerateLimit(60);

    std::vector<ScenarioPreset> available_presets = presets_;
    if (available_presets.empty()) {
        ScenarioPreset current;
        current.name = "Current Scene";
        current.bodies = sim.get_bodies();
        current.gravity = sim.get_gravity();
        current.dt = sim.get_dt();
        current.integrator = sim.get_integrator();
        current.substeps = sim.get_substeps();
        current.meters_to_pixels = scale_;
        available_presets.push_back(current);
    }

    std::size_t current_preset_idx = 0;
    std::string preset_name = available_presets.front().name;

    const auto sync_selected_index = [&](const Simulator& current_sim) {
        const int count = static_cast<int>(current_sim.get_bodies().size());
        if (count == 0) {
            selected_body_idx_ = -1;
        } else if (selected_body_idx_ < 0 || selected_body_idx_ >= count) {
            selected_body_idx_ = 0;
        }
    };

        const auto apply_preset = [&](std::size_t idx) {
        current_preset_idx = idx;
        const ScenarioPreset& preset = available_presets[idx];
        preset_name = preset.name;
        sim.set_gravity(preset.gravity);
        sim.set_integrator(preset.integrator);
        sim.set_dt(preset.dt * time_scale_);
        sim.set_substeps(preset.substeps);
        sim.set_bodies(preset.bodies);
        sim.reset_time();
        scale_ = preset.meters_to_pixels;
        camera_center_ = Vec2{0.0, 0.0};
        zoom_ = 1.0;
        paused_ = false;
        collision_active_ = false;
        history_frames_.clear();
        custom_body_counter_ = preset.bodies.size() + 1;
        rebuild_trails(preset.bodies.size());
        sync_selected_index(sim);
    };

    apply_preset(0);
    cursor_world_ = Vec2{0.0, 0.0};

    while (window.isOpen()) {
        bool request_single_step = false;

#if SFML_VERSION_MAJOR >= 3
        while (const std::optional event_opt = window.pollEvent()) {
            const sf::Event& event = *event_opt;
            if (event.is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* resized = event.getIf<sf::Event::Resized>()) {
                sf::FloatRect visibleArea(
                    {0.f, 0.f},
                    {static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)});
                window.setView(sf::View(visibleArea));
                width_ = resized->size.x;
                height_ = resized->size.y;
            } else if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
                if (dragging_view_) {
                    const sf::Vector2i delta = moved->position - last_mouse_pos_;
                    camera_center_.x -= pixels_to_meters(static_cast<double>(delta.x), scale_ * zoom_);
                    camera_center_.y += pixels_to_meters(static_cast<double>(delta.y), scale_ * zoom_);
                    last_mouse_pos_ = moved->position;
                }
                cursor_world_ = screen_to_world(sf::Vector2f{
                    static_cast<float>(moved->position.x),
                    static_cast<float>(moved->position.y),
                });
            } else if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Right) {
                    dragging_view_ = true;
                    last_mouse_pos_ = mouse->position;
                    continue;
                }
                if (mouse->button != sf::Mouse::Button::Left) {
                    continue;
                }
                const sf::Vector2f mouse_pos{
                    static_cast<float>(mouse->position.x),
                    static_cast<float>(mouse->position.y),
                };
                cursor_world_ = screen_to_world(mouse_pos);
                if (mouse_pos.x < static_cast<float>(width_) - side_panel_width_) {
                    const auto& bodies = sim.get_bodies();
                    float best_dist2 = std::numeric_limits<float>::max();
                    int best_idx = -1;
                    for (std::size_t i = 0; i < bodies.size(); ++i) {
                        const sf::Vector2f p = world_to_screen(bodies[i].pos);
                        const float dx = p.x - mouse_pos.x;
                        const float dy = p.y - mouse_pos.y;
                        const float dist2 = dx * dx + dy * dy;
                        const float hit = static_cast<float>(bodies[i].radius + 8.0);
                        if (dist2 <= hit * hit && dist2 < best_dist2) {
                            best_dist2 = dist2;
                            best_idx = static_cast<int>(i);
                        }
                    }
                    selected_body_idx_ = best_idx;
                }
            } else if (const auto* mouse = event.getIf<sf::Event::MouseButtonReleased>()) {
                if (mouse->button == sf::Mouse::Button::Right) {
                    dragging_view_ = false;
                }
            } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    window.close();
                } else if (key->code == sf::Keyboard::Key::Space) {
                    if (!collision_active_) {
                        paused_ = !paused_;
                    }
                } else if (key->code == sf::Keyboard::Key::Period) {
                    if (paused_ && !collision_active_) {
                        request_single_step = true;
                    }
                } else if (key->code == sf::Keyboard::Key::R) {
                    apply_preset(current_preset_idx);
                } else if (key->code == sf::Keyboard::Key::F) {
                    field_mode_ = static_cast<FieldMode>((static_cast<int>(field_mode_) + 1) % 3);
                } else if (key->code == sf::Keyboard::Key::I) {
                    sim.set_integrator(sim.get_integrator() == Integrator::RK4 ? Integrator::Euler
                                                                              : Integrator::RK4);
                } else if (key->code == sf::Keyboard::Key::P) {
                    show_prediction_ = !show_prediction_;
                } else if (key->code == sf::Keyboard::Key::V) {
                    show_vectors_ = !show_vectors_;
                } else if (key->code == sf::Keyboard::Key::M) {
                    collision_mode_ = static_cast<CollisionMode>((static_cast<int>(collision_mode_) + 1) % 4);
                } else if (key->code == sf::Keyboard::Key::Add ||
                           key->code == sf::Keyboard::Key::Equal) {
                    time_scale_ = std::min(64.0, time_scale_ * 2.0);
                    sim.set_dt(available_presets[current_preset_idx].dt * time_scale_);
                } else if (key->code == sf::Keyboard::Key::Subtract ||
                           key->code == sf::Keyboard::Key::Hyphen) {
                    time_scale_ = std::max(0.0625, time_scale_ * 0.5);
                    sim.set_dt(available_presets[current_preset_idx].dt * time_scale_);
                } else if (key->code >= sf::Keyboard::Key::Num1 &&
                           key->code <= sf::Keyboard::Key::Num9) {
                    const std::size_t idx = static_cast<std::size_t>(static_cast<int>(key->code) -
                                                                     static_cast<int>(sf::Keyboard::Key::Num1));
                    if (idx < available_presets.size()) {
                        apply_preset(idx);
                    }
                } else if (key->code == sf::Keyboard::Key::N) {
                    auto& bodies = sim.access_bodies();
                    Body body(
                        5.0e24,
                        cursor_world_,
                        Vec2{0.0, 0.0},
                        8.0,
                        next_editor_color(custom_body_counter_),
                        false,
                        false,
                        "Custom" + std::to_string(custom_body_counter_));
                    ++custom_body_counter_;
                    bodies.push_back(body);
                    trails_.emplace_back();
                    selected_body_idx_ = static_cast<int>(bodies.size() - 1);
                } else if ((key->code == sf::Keyboard::Key::Delete ||
                            key->code == sf::Keyboard::Key::Backspace) &&
                           selected_body_idx_ >= 0) {
                    auto& bodies = sim.access_bodies();
                    const std::size_t idx = static_cast<std::size_t>(selected_body_idx_);
                    if (idx < bodies.size()) {
                        bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(idx));
                        if (idx < trails_.size()) {
                            trails_.erase(trails_.begin() + static_cast<std::ptrdiff_t>(idx));
                        }
                    }
                    sync_selected_index(sim);
                } else if (selected_body_idx_ >= 0) {
                    auto& bodies = sim.access_bodies();
                    if (selected_body_idx_ < static_cast<int>(bodies.size())) {
                        Body& body = bodies[static_cast<std::size_t>(selected_body_idx_)];
                        const double move_step = 2.0e9;
                        const double velocity_step = 250.0;
                        if (key->code == sf::Keyboard::Key::Up) {
                            body.pos.y += move_step;
                        } else if (key->code == sf::Keyboard::Key::Down) {
                            body.pos.y -= move_step;
                        } else if (key->code == sf::Keyboard::Key::Left) {
                            body.pos.x -= move_step;
                        } else if (key->code == sf::Keyboard::Key::Right) {
                            body.pos.x += move_step;
                        } else if (key->code == sf::Keyboard::Key::W) {
                            body.vel.y += velocity_step;
                        } else if (key->code == sf::Keyboard::Key::S) {
                            body.vel.y -= velocity_step;
                        } else if (key->code == sf::Keyboard::Key::A) {
                            body.vel.x -= velocity_step;
                        } else if (key->code == sf::Keyboard::Key::D) {
                            body.vel.x += velocity_step;
                        } else if (key->code == sf::Keyboard::Key::Q) {
                            body.mass = std::max(1.0, body.mass * 0.8);
                        } else if (key->code == sf::Keyboard::Key::E) {
                            body.mass *= 1.25;
                        } else if (key->code == sf::Keyboard::Key::Z) {
                            body.radius = std::max(2.0, body.radius - 1.0);
                        } else if (key->code == sf::Keyboard::Key::X) {
                            body.radius += 1.0;
                        } else if (key->code == sf::Keyboard::Key::T) {
                            body.is_star = !body.is_star;
                            if (body.is_star) {
                                body.is_satellite = false;
                            }
                        } else if (key->code == sf::Keyboard::Key::Y) {
                            body.is_satellite = !body.is_satellite;
                            if (body.is_satellite) {
                                body.is_star = false;
                            }
                        }
                    }
                }

                if (key->code == sf::Keyboard::Key::C && collision_active_) {
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
                    sync_selected_index(sim);
                }
            }
        }
#else
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(
                    0.f, 0.f,
                    static_cast<float>(event.size.width),
                    static_cast<float>(event.size.height));
                window.setView(sf::View(visibleArea));
                width_ = event.size.width;
                height_ = event.size.height;
            } else if (event.type == sf::Event::MouseMoved) {
                if (dragging_view_) {
                    const sf::Vector2i current{event.mouseMove.x, event.mouseMove.y};
                    const sf::Vector2i delta = current - last_mouse_pos_;
                    camera_center_.x -= pixels_to_meters(static_cast<double>(delta.x), scale_ * zoom_);
                    camera_center_.y += pixels_to_meters(static_cast<double>(delta.y), scale_ * zoom_);
                    last_mouse_pos_ = current;
                }
                cursor_world_ = screen_to_world(sf::Vector2f{
                    static_cast<float>(event.mouseMove.x),
                    static_cast<float>(event.mouseMove.y),
                });
            } else if (event.type == sf::Event::MouseButtonPressed &&
                       event.mouseButton.button == sf::Mouse::Left) {
                const sf::Vector2f mouse_pos{
                    static_cast<float>(event.mouseButton.x),
                    static_cast<float>(event.mouseButton.y),
                };
                cursor_world_ = screen_to_world(mouse_pos);
                if (mouse_pos.x < static_cast<float>(width_) - side_panel_width_) {
                    const auto& bodies = sim.get_bodies();
                    float best_dist2 = std::numeric_limits<float>::max();
                    int best_idx = -1;
                    for (std::size_t i = 0; i < bodies.size(); ++i) {
                        const sf::Vector2f p = world_to_screen(bodies[i].pos);
                        const float dx = p.x - mouse_pos.x;
                        const float dy = p.y - mouse_pos.y;
                        const float dist2 = dx * dx + dy * dy;
                        const float hit = static_cast<float>(bodies[i].radius + 8.0);
                        if (dist2 <= hit * hit && dist2 < best_dist2) {
                            best_dist2 = dist2;
                            best_idx = static_cast<int>(i);
                        }
                    }
                    selected_body_idx_ = best_idx;
                }
            } else if (event.type == sf::Event::MouseButtonPressed &&
                       event.mouseButton.button == sf::Mouse::Right) {
                dragging_view_ = true;
                last_mouse_pos_ = sf::Vector2i{event.mouseButton.x, event.mouseButton.y};
            } else if (event.type == sf::Event::MouseButtonReleased &&
                       event.mouseButton.button == sf::Mouse::Right) {
                dragging_view_ = false;
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    window.close();
                } else if (event.key.code == sf::Keyboard::Space) {
                    if (!collision_active_) {
                        paused_ = !paused_;
                    }
                } else if (event.key.code == sf::Keyboard::Period) {
                    if (paused_ && !collision_active_) {
                        request_single_step = true;
                    }
                } else if (event.key.code == sf::Keyboard::R) {
                    apply_preset(current_preset_idx);
                } else if (event.key.code == sf::Keyboard::F) {
                    field_mode_ = static_cast<FieldMode>((static_cast<int>(field_mode_) + 1) % 3);
                } else if (event.key.code == sf::Keyboard::I) {
                    sim.set_integrator(sim.get_integrator() == Integrator::RK4 ? Integrator::Euler
                                                                              : Integrator::RK4);
                } else if (event.key.code == sf::Keyboard::P) {
                    show_prediction_ = !show_prediction_;
                } else if (event.key.code == sf::Keyboard::V) {
                    show_vectors_ = !show_vectors_;
                } else if (event.key.code == sf::Keyboard::M) {
                    collision_mode_ = static_cast<CollisionMode>((static_cast<int>(collision_mode_) + 1) % 4);
                } else if (event.key.code == sf::Keyboard::Add ||
                           event.key.code == sf::Keyboard::Equal) {
                    time_scale_ = std::min(64.0, time_scale_ * 2.0);
                    sim.set_dt(available_presets[current_preset_idx].dt * time_scale_);
                } else if (event.key.code == sf::Keyboard::Subtract ||
                           event.key.code == sf::Keyboard::Hyphen) {
                    time_scale_ = std::max(0.0625, time_scale_ * 0.5);
                    sim.set_dt(available_presets[current_preset_idx].dt * time_scale_);
                } else if (event.key.code >= sf::Keyboard::Num1 &&
                           event.key.code <= sf::Keyboard::Num9) {
                    const std::size_t idx = static_cast<std::size_t>(event.key.code - sf::Keyboard::Num1);
                    if (idx < available_presets.size()) {
                        apply_preset(idx);
                    }
                } else if (event.key.code == sf::Keyboard::N) {
                    auto& bodies = sim.access_bodies();
                    Body body(
                        5.0e24,
                        cursor_world_,
                        Vec2{0.0, 0.0},
                        8.0,
                        next_editor_color(custom_body_counter_),
                        false,
                        false,
                        "Custom" + std::to_string(custom_body_counter_));
                    ++custom_body_counter_;
                    bodies.push_back(body);
                    trails_.emplace_back();
                    selected_body_idx_ = static_cast<int>(bodies.size() - 1);
                } else if ((event.key.code == sf::Keyboard::Delete ||
                            event.key.code == sf::Keyboard::BackSpace) &&
                           selected_body_idx_ >= 0) {
                    auto& bodies = sim.access_bodies();
                    const std::size_t idx = static_cast<std::size_t>(selected_body_idx_);
                    if (idx < bodies.size()) {
                        bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(idx));
                        if (idx < trails_.size()) {
                            trails_.erase(trails_.begin() + static_cast<std::ptrdiff_t>(idx));
                        }
                    }
                    sync_selected_index(sim);
                } else if (selected_body_idx_ >= 0) {
                    auto& bodies = sim.access_bodies();
                    if (selected_body_idx_ < static_cast<int>(bodies.size())) {
                        Body& body = bodies[static_cast<std::size_t>(selected_body_idx_)];
                        const double move_step = 2.0e9;
                        const double velocity_step = 250.0;
                        if (event.key.code == sf::Keyboard::Up) {
                            body.pos.y += move_step;
                        } else if (event.key.code == sf::Keyboard::Down) {
                            body.pos.y -= move_step;
                        } else if (event.key.code == sf::Keyboard::Left) {
                            body.pos.x -= move_step;
                        } else if (event.key.code == sf::Keyboard::Right) {
                            body.pos.x += move_step;
                        } else if (event.key.code == sf::Keyboard::W) {
                            body.vel.y += velocity_step;
                        } else if (event.key.code == sf::Keyboard::S) {
                            body.vel.y -= velocity_step;
                        } else if (event.key.code == sf::Keyboard::A) {
                            body.vel.x -= velocity_step;
                        } else if (event.key.code == sf::Keyboard::D) {
                            body.vel.x += velocity_step;
                        } else if (event.key.code == sf::Keyboard::Q) {
                            body.mass = std::max(1.0, body.mass * 0.8);
                        } else if (event.key.code == sf::Keyboard::E) {
                            body.mass *= 1.25;
                        } else if (event.key.code == sf::Keyboard::Z) {
                            body.radius = std::max(2.0, body.radius - 1.0);
                        } else if (event.key.code == sf::Keyboard::X) {
                            body.radius += 1.0;
                        } else if (event.key.code == sf::Keyboard::T) {
                            body.is_star = !body.is_star;
                            if (body.is_star) {
                                body.is_satellite = false;
                            }
                        } else if (event.key.code == sf::Keyboard::Y) {
                            body.is_satellite = !body.is_satellite;
                            if (body.is_satellite) {
                                body.is_star = false;
                            }
                        }
                    }
                }

                if (event.key.code == sf::Keyboard::C && collision_active_) {
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
                    sync_selected_index(sim);
                }
            }
        }
#endif

        if ((!paused_ || request_single_step) && !collision_active_) {
            sim.step();
        }

        export_simulation_data(sim);
        update_window_title(window, sim, preset_name);

        window.clear(sf::Color(8, 10, 18));
        draw_field_overlay(window, sim);

        const auto& bodies = sim.get_bodies();

        std::vector<std::size_t> to_remove;
        bool scene_changed = false;
        if (!collision_active_) {
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
                if (collision_active_) {
                    break;
                }
                if (scene_changed) {
                    break;
                }
            }
        }

        if (!to_remove.empty()) {
            remove_bodies_by_index(sim, to_remove);
            sync_selected_index(sim);
        }

        draw_body_trails(window, sim);
        draw_bodies(window, sim);
        draw_prediction_path(window, sim);
        draw_body_vectors(window, sim);
        draw_editor_cursor(window, cursor_world_);
        draw_side_panel(window, sim, preset_name);
        window.display();
    }
}

} // namespace orbitsimlite
