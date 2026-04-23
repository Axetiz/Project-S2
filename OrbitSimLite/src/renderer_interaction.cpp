// OrbitSimLite - Renderer interaction and scene control
#include "renderer.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>

namespace orbitsimlite {

namespace {

// Scene file names are shared with the export system, so keep the rule simple
// and deterministic: allow only alphanumeric characters in saved filenames.
std::string sanitize_filename_token(std::string value) {
    for (char& ch : value) {
        const bool ok =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9');
        if (!ok) {
            ch = '_';
        }
    }
    if (value.empty()) {
        return "Scene";
    }
    return value;
}

bool delete_scene_file(const std::string& directory, const std::string& filename) {
    std::error_code ec;
    return std::filesystem::remove(std::filesystem::path(directory) / filename, ec) && !ec;
}

// Match the palette used for newly created editor bodies so manually built
// scenes stay visually distinct without extra user work.
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

void Renderer::sync_selected_index(const Simulator& sim) {
    const int count = static_cast<int>(sim.get_bodies().size());
    if (count == 0) {
        selected_body_idx_ = -1;
    } else if (selected_body_idx_ < 0 || selected_body_idx_ >= count) {
        selected_body_idx_ = 0;
    }
}

void Renderer::apply_scene(Simulator& sim, const ScenarioPreset& scene) {
    // Treat scene application as a full reset of the interactive session so
    // trails, camera, export labels, and collision prompts stay coherent.
    current_scene_ = scene;
    current_preset_name_ = current_scene_.name;
    current_export_label_ = current_scene_.name;
    refresh_export_targets();
    sim.set_gravity(current_scene_.gravity);
    sim.set_integrator(current_scene_.integrator);
    sim.set_dt(current_scene_.dt * time_scale_);
    sim.set_substeps(current_scene_.substeps);
    sim.set_bodies(current_scene_.bodies);
    sim.reset_time();
    scale_ = current_scene_.meters_to_pixels;
    camera_center_ = Vec2{0.0, 0.0};
    zoom_ = 1.0;
    paused_ = false;
    collision_active_ = false;
    custom_body_counter_ = current_scene_.bodies.size() + 1;
    rebuild_trails(current_scene_.bodies.size());
    sync_selected_index(sim);
}

void Renderer::apply_preset(Simulator& sim, std::size_t idx) {
    current_preset_idx_ = idx;
    apply_scene(sim, presets_[idx]);
}

void Renderer::handle_mouse_move(const sf::Vector2i& position) {
    // Camera dragging works entirely in world space so panning never mutates
    // simulation data or corrupts orbit trails.
    if (dragging_view_) {
        const sf::Vector2i delta = position - last_mouse_pos_;
        camera_center_.x -= pixels_to_meters(static_cast<double>(delta.x), scale_ * zoom_);
        camera_center_.y += pixels_to_meters(static_cast<double>(delta.y), scale_ * zoom_);
        last_mouse_pos_ = position;
    }
    cursor_world_ = screen_to_world(sf::Vector2f{
        static_cast<float>(position.x),
        static_cast<float>(position.y),
    });
}

void Renderer::handle_left_click(const sf::Vector2f& mouse_pos, const Simulator& sim) {
    // Ignore clicks inside the side panel; selection should only happen inside
    // the simulation canvas.
    cursor_world_ = screen_to_world(mouse_pos);
    if (mouse_pos.x >= static_cast<float>(width_) - side_panel_width_) {
        return;
    }

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

void Renderer::begin_view_drag(const sf::Vector2i& position) {
    dragging_view_ = true;
    last_mouse_pos_ = position;
}

void Renderer::end_view_drag() {
    dragging_view_ = false;
}

void Renderer::handle_text_input(std::uint32_t unicode) {
    if (scene_io_mode_ != SceneIoMode::Save) {
        return;
    }
    if (unicode >= 32 && unicode < 127 && scene_io_input_.size() < 48) {
        scene_io_input_ += static_cast<char>(unicode);
        scene_io_confirm_overwrite_ = false;
    }
}

bool Renderer::handle_scene_io_key(sf::Keyboard::Key key, Simulator& sim) {
    const auto is_backspace = [&](sf::Keyboard::Key candidate) {
#if SFML_VERSION_MAJOR >= 3
        return candidate == sf::Keyboard::Key::Backspace;
#else
        return candidate == sf::Keyboard::BackSpace;
#endif
    };
    const auto is_confirm = [&](sf::Keyboard::Key candidate) {
#if SFML_VERSION_MAJOR >= 3
        return candidate == sf::Keyboard::Key::Enter;
#else
        return candidate == sf::Keyboard::Enter || candidate == sf::Keyboard::Return;
#endif
    };

    if (scene_io_mode_ == SceneIoMode::None) {
        return false;
    }

    if (key == sf::Keyboard::Key::Escape) {
        scene_io_mode_ = SceneIoMode::None;
        scene_io_confirm_overwrite_ = false;
        scene_io_confirm_delete_ = false;
        return true;
    }

    if (scene_io_mode_ == SceneIoMode::Save) {
        // Export mode behaves like a tiny filename editor with overwrite
        // protection instead of silently replacing an existing scene file.
        if (is_backspace(key) && !scene_io_input_.empty()) {
            scene_io_input_.pop_back();
            scene_io_confirm_overwrite_ = false;
        } else if (key == sf::Keyboard::Key::Y && scene_io_confirm_overwrite_) {
            scene_io_message_ = save_scene_json(sim, scene_io_input_)
                ? "Saved to Exports/" + sanitize_filename_token(scene_io_input_) + ".json"
                : "Save failed";
            refresh_saved_scene_files();
            scene_io_mode_ = SceneIoMode::None;
            scene_io_confirm_overwrite_ = false;
        } else if (is_confirm(key) && !scene_io_input_.empty()) {
            const std::filesystem::path scene_path =
                std::filesystem::path(scene_directory_) /
                (sanitize_filename_token(scene_io_input_) + ".json");
            if (std::filesystem::exists(scene_path) && !scene_io_confirm_overwrite_) {
                scene_io_confirm_overwrite_ = true;
            } else {
                scene_io_message_ = save_scene_json(sim, scene_io_input_)
                    ? "Saved to Exports/" + sanitize_filename_token(scene_io_input_) + ".json"
                    : "Save failed";
                refresh_saved_scene_files();
                scene_io_mode_ = SceneIoMode::None;
                scene_io_confirm_overwrite_ = false;
            }
        }
        return true;
    }

    if (scene_io_mode_ == SceneIoMode::Load) {
        // Load mode doubles as a scene manager: browse, load, or delete
        // previously saved scenes from the same overlay.
        if (key == sf::Keyboard::Key::Up && !saved_scene_files_.empty()) {
            selected_saved_scene_idx_ = std::max(0, selected_saved_scene_idx_ - 1);
            scene_io_confirm_delete_ = false;
        } else if (key == sf::Keyboard::Key::Down && !saved_scene_files_.empty()) {
            selected_saved_scene_idx_ =
                std::min(static_cast<int>(saved_scene_files_.size()) - 1, selected_saved_scene_idx_ + 1);
            scene_io_confirm_delete_ = false;
        } else if ((key == sf::Keyboard::Key::Delete || is_backspace(key)) && !saved_scene_files_.empty()) {
            scene_io_confirm_delete_ = true;
        } else if (key == sf::Keyboard::Key::Y && scene_io_confirm_delete_ && !saved_scene_files_.empty()) {
            const std::string filename =
                saved_scene_files_[static_cast<std::size_t>(selected_saved_scene_idx_)];
            if (delete_scene_file(scene_directory_, filename)) {
                scene_io_message_ = "Deleted Exports/" + filename;
                refresh_saved_scene_files();
                scene_io_confirm_delete_ = false;
                if (saved_scene_files_.empty()) {
                    scene_io_mode_ = SceneIoMode::None;
                }
            } else {
                scene_io_message_ = "Delete failed";
                scene_io_confirm_delete_ = false;
            }
        } else if (is_confirm(key) && !saved_scene_files_.empty()) {
            ScenarioPreset loaded_scene;
            if (load_scene_json(saved_scene_files_[static_cast<std::size_t>(selected_saved_scene_idx_)], loaded_scene)) {
                apply_scene(sim, loaded_scene);
                scene_io_message_ = "Loaded " + loaded_scene.name;
            } else {
                scene_io_message_ = "Load failed";
            }
            scene_io_mode_ = SceneIoMode::None;
            scene_io_confirm_delete_ = false;
        }
        return true;
    }

    return true;
}

void Renderer::handle_body_creation(Simulator& sim) {
    // New bodies inherit the current cursor position and a default mass/radius
    // so the user can build a scene incrementally without opening another UI.
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
}

void Renderer::handle_selected_body_removal(Simulator& sim) {
    if (selected_body_idx_ < 0) {
        return;
    }

    auto& bodies = sim.access_bodies();
    const std::size_t idx = static_cast<std::size_t>(selected_body_idx_);
    if (idx < bodies.size()) {
        bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(idx));
        if (idx < trails_.size()) {
            trails_.erase(trails_.begin() + static_cast<std::ptrdiff_t>(idx));
        }
    }
    sync_selected_index(sim);
}

void Renderer::handle_selected_body_edit(sf::Keyboard::Key key, Simulator& sim) {
    if (selected_body_idx_ < 0) {
        return;
    }

    auto& bodies = sim.access_bodies();
    if (selected_body_idx_ >= static_cast<int>(bodies.size())) {
        return;
    }

    Body& body = bodies[static_cast<std::size_t>(selected_body_idx_)];
    // Keep editor increments modest so keyboard nudges remain predictable for
    // both astronomical presets and manually created scenes.
    const double move_step = 2.0e9;
    const double velocity_step = 250.0;
    if (key == sf::Keyboard::Key::Up) {
        body.pos.y += move_step;
    } else if (key == sf::Keyboard::Key::Down) {
        body.pos.y -= move_step;
    } else if (key == sf::Keyboard::Key::Left) {
        body.pos.x -= move_step;
    } else if (key == sf::Keyboard::Key::Right) {
        body.pos.x += move_step;
    } else if (key == sf::Keyboard::Key::W) {
        body.vel.y += velocity_step;
    } else if (key == sf::Keyboard::Key::S) {
        body.vel.y -= velocity_step;
    } else if (key == sf::Keyboard::Key::A) {
        body.vel.x -= velocity_step;
    } else if (key == sf::Keyboard::Key::D) {
        body.vel.x += velocity_step;
    } else if (key == sf::Keyboard::Key::Q) {
        body.mass = std::max(1.0, body.mass * 0.8);
    } else if (key == sf::Keyboard::Key::E) {
        body.mass *= 1.25;
    } else if (key == sf::Keyboard::Key::Z) {
        body.radius = std::max(2.0, body.radius - 1.0);
    } else if (key == sf::Keyboard::Key::X) {
        body.radius += 1.0;
    } else if (key == sf::Keyboard::Key::T) {
        body.is_star = !body.is_star;
        if (body.is_star) {
            body.is_satellite = false;
        }
    } else if (key == sf::Keyboard::Key::Y) {
        body.is_satellite = !body.is_satellite;
        if (body.is_satellite) {
            body.is_star = false;
        }
    }
}

void Renderer::handle_main_key(sf::Keyboard::Key key, Simulator& sim, bool& request_single_step) {
    const auto is_backspace = [&](sf::Keyboard::Key candidate) {
#if SFML_VERSION_MAJOR >= 3
        return candidate == sf::Keyboard::Key::Backspace;
#else
        return candidate == sf::Keyboard::BackSpace;
#endif
    };

    // These key bindings are intentionally grouped by user intent:
    // playback, solver/field toggles, scene I/O, scaling, and body editing.
    if (key == sf::Keyboard::Key::Space) {
        if (!collision_active_) {
            paused_ = !paused_;
        }
    } else if (key == sf::Keyboard::Key::Period) {
        if (paused_ && !collision_active_) {
            request_single_step = true;
        }
    } else if (key == sf::Keyboard::Key::R) {
        apply_preset(sim, current_preset_idx_);
    } else if (key == sf::Keyboard::Key::F) {
        field_mode_ = static_cast<FieldMode>((static_cast<int>(field_mode_) + 1) % 3);
    } else if (key == sf::Keyboard::Key::I) {
        sim.set_integrator(sim.get_integrator() == Integrator::RK4 ? Integrator::Euler
                                                                  : Integrator::RK4);
    } else if (key == sf::Keyboard::Key::P) {
        show_prediction_ = !show_prediction_;
    } else if (key == sf::Keyboard::Key::V) {
        show_vectors_ = !show_vectors_;
    } else if (key == sf::Keyboard::Key::M) {
        collision_mode_ = static_cast<CollisionMode>((static_cast<int>(collision_mode_) + 1) % 4);
    } else if (key == sf::Keyboard::Key::K) {
        scene_io_mode_ = SceneIoMode::Save;
        scene_io_input_ = sanitize_filename_token(current_export_label_);
        scene_io_confirm_overwrite_ = false;
    } else if (key == sf::Keyboard::Key::L) {
        refresh_saved_scene_files();
        scene_io_mode_ = SceneIoMode::Load;
        scene_io_confirm_delete_ = false;
    } else if (key == sf::Keyboard::Key::Add || key == sf::Keyboard::Key::Equal) {
        time_scale_ = std::min(64.0, time_scale_ * 2.0);
        sim.set_dt(current_scene_.dt * time_scale_);
    } else if (key == sf::Keyboard::Key::Subtract || key == sf::Keyboard::Key::Hyphen) {
        time_scale_ = std::max(0.0625, time_scale_ * 0.5);
        sim.set_dt(current_scene_.dt * time_scale_);
    } else if (key == sf::Keyboard::Key::N) {
        handle_body_creation(sim);
    } else if (key == sf::Keyboard::Key::Delete || is_backspace(key)) {
        handle_selected_body_removal(sim);
    } else {
        handle_selected_body_edit(key, sim);
    }

    if (key == sf::Keyboard::Key::C && collision_active_) {
        confirm_collision_prompt(sim);
    }
}

} // namespace orbitsimlite
