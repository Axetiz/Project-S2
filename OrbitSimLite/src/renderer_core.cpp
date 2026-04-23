// OrbitSimLite - Renderer core orchestration
#include "renderer.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <thread>

namespace orbitsimlite {

namespace {

// Used only for the UI title/panel so long-running scenes are easier to read.
constexpr double kEarthYearSeconds = 365.25 * 24.0 * 3600.0;

} // namespace

Renderer::Renderer(unsigned width, unsigned height, double meters_to_pixels)
    : width_(width), height_(height), scale_(meters_to_pixels) {}

void Renderer::set_presets(const std::vector<ScenarioPreset>& presets) {
    presets_ = presets;
}

void Renderer::set_output_options(const OutputOptions& options) {
    output_options_ = options;
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

std::string Renderer::integrator_name(Integrator integrator) const {
    switch (integrator) {
    case Integrator::Euler:
        return "Euler";
    case Integrator::RK4:
        return "RK4";
    }
    return "Unknown";
}

void Renderer::run(Simulator& sim) {
    ensure_font_loaded();

    // Create the explorer window lazily so headless export can reuse the same
    // Renderer type without touching any SFML windowing code.
#if SFML_VERSION_MAJOR >= 3
    sf::RenderWindow window(sf::VideoMode({width_, height_}), "OrbitSimLite");
#else
    sf::RenderWindow window(sf::VideoMode(width_, height_), "OrbitSimLite");
#endif
    window.setFramerateLimit(60);

    if (presets_.empty()) {
        // If the caller did not provide presets, wrap the current simulator
        // state into a synthetic scene so reset/export still behave naturally.
        ScenarioPreset current;
        current.name = "Current Scene";
        current.bodies = sim.get_bodies();
        current.gravity = sim.get_gravity();
        current.dt = sim.get_dt();
        current.integrator = sim.get_integrator();
        current.substeps = sim.get_substeps();
        current.meters_to_pixels = scale_;
        presets_.push_back(current);
    }

    current_preset_idx_ = 0;
    apply_preset(sim, current_preset_idx_);
    cursor_world_ = Vec2{0.0, 0.0};

    if (output_options_.enable_json || output_options_.enable_csv) {
        std::cout << "Export path: " << export_directory_ << "/\n";
    }

    while (window.isOpen()) {
        bool request_single_step = false;

        // Stage 1: gather user input and update editor state.
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
                handle_mouse_move(moved->position);
            } else if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Right) {
                    begin_view_drag(mouse->position);
                } else if (mouse->button == sf::Mouse::Button::Left) {
                    handle_left_click(
                        sf::Vector2f{
                            static_cast<float>(mouse->position.x),
                            static_cast<float>(mouse->position.y),
                        },
                        sim);
                }
            } else if (const auto* mouse = event.getIf<sf::Event::MouseButtonReleased>()) {
                if (mouse->button == sf::Mouse::Button::Right) {
                    end_view_drag();
                }
            } else if (const auto* text = event.getIf<sf::Event::TextEntered>()) {
                handle_text_input(text->unicode);
            } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
                if (!handle_scene_io_key(key->code, sim)) {
                    if (key->code == sf::Keyboard::Key::Escape) {
                        window.close();
                    } else {
                        handle_main_key(key->code, sim, request_single_step);
                    }
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
                handle_mouse_move(sf::Vector2i{event.mouseMove.x, event.mouseMove.y});
            } else if (event.type == sf::Event::MouseButtonPressed &&
                       event.mouseButton.button == sf::Mouse::Left) {
                handle_left_click(
                    sf::Vector2f{
                        static_cast<float>(event.mouseButton.x),
                        static_cast<float>(event.mouseButton.y),
                    },
                    sim);
            } else if (event.type == sf::Event::MouseButtonPressed &&
                       event.mouseButton.button == sf::Mouse::Right) {
                begin_view_drag(sf::Vector2i{event.mouseButton.x, event.mouseButton.y});
            } else if (event.type == sf::Event::MouseButtonReleased &&
                       event.mouseButton.button == sf::Mouse::Right) {
                end_view_drag();
            } else if (event.type == sf::Event::TextEntered) {
                handle_text_input(event.text.unicode);
            } else if (event.type == sf::Event::KeyPressed) {
                if (!handle_scene_io_key(event.key.code, sim)) {
                    if (event.key.code == sf::Keyboard::Escape) {
                        window.close();
                    } else {
                        handle_main_key(event.key.code, sim, request_single_step);
                    }
                }
            }
        }
#endif

        // Stage 2: advance the simulation only when the explorer is not paused
        // or when the user explicitly requested a single-step update.
        if ((!paused_ || request_single_step) && !collision_active_) {
            sim.step();
        }

        // Stage 3: refresh external outputs and redraw the complete frame.
        export_simulation_data(sim);
        update_window_title(window, sim, current_preset_name_);

        window.clear(sf::Color(8, 10, 18));
        draw_field_overlay(window, sim);
        resolve_scene_collisions(sim);
        draw_body_trails(window, sim);
        draw_bodies(window, sim);
        draw_prediction_path(window, sim);
        draw_body_vectors(window, sim);
        draw_editor_cursor(window, cursor_world_);
        draw_side_panel(window, sim, current_preset_name_);
        draw_scene_io_overlay(window);
        window.display();
    }
}

void Renderer::run_headless(Simulator& sim,
                            std::optional<double> real_time_seconds,
                            const std::string& preset_name) {
    // Headless mode reuses the exact same export path logic as the interactive
    // mode, but intentionally skips all window creation and drawing work.
    current_export_label_ = preset_name;
    refresh_export_targets();
    rebuild_trails(sim.get_bodies().size());

    std::cout << "Running headless export mode for preset: " << preset_name << "\n";
    std::cout << "Export path: " << export_directory_ << "/\n";
    std::cout << "Outputs:";
    if (output_options_.enable_json) {
        std::cout << " JSON";
    }
    if (output_options_.enable_csv) {
        std::cout << " CSV";
    }
    std::cout << "\n";
    if (output_options_.enable_json) {
        std::cout << "JSON file: " << current_json_export_path_ << "\n";
    }
    if (output_options_.enable_csv) {
        std::cout << "CSV file: " << current_csv_export_path_ << "\n";
    }

    const auto start = std::chrono::steady_clock::now();
    auto last_export = start;
    constexpr auto export_interval = std::chrono::milliseconds(33);
    export_simulation_data(sim);
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (real_time_seconds.has_value()) {
            const double elapsed = std::chrono::duration<double>(now - start).count();
            if (elapsed >= *real_time_seconds) {
                break;
            }
        }
        sim.step();
        // Throttle file writes to a steady cadence so other tools can consume
        // the snapshot stream without the exporter hammering the filesystem.
        if (now - last_export >= export_interval) {
            export_simulation_data(sim);
            last_export = now;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    export_simulation_data(sim);

    std::cout << "Headless run finished. Final simulated time="
              << sim.get_time() << " seconds.\n";
}

} // namespace orbitsimlite
