// OrbitSimLite - Renderer scene and scene-file management
#include "renderer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace orbitsimlite {

namespace {

// Scene file names are user-facing, so keep them simple, portable, and stable
// across export/import, regardless of keyboard layout or punctuation.
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

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

bool extract_string_field(const std::string& text, const std::string& key, std::string& out) {
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }
    out = match[1].str();
    return true;
}

bool extract_double_field(const std::string& text, const std::string& key, double& out) {
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }
    out = std::stod(match[1].str());
    return true;
}

bool extract_int_field(const std::string& text, const std::string& key, int& out) {
    double temp = 0.0;
    if (!extract_double_field(text, key, temp)) {
        return false;
    }
    out = static_cast<int>(temp);
    return true;
}

bool extract_bool_field(const std::string& text, const std::string& key, bool& out) {
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }
    out = match[1].str() == "true";
    return true;
}

std::vector<std::string> split_top_level_objects(const std::string& array_text) {
    std::vector<std::string> objects;
    int depth = 0;
    bool in_string = false;
    std::size_t object_start = std::string::npos;
    for (std::size_t i = 0; i < array_text.size(); ++i) {
        const char ch = array_text[i];
        if (ch == '"' && (i == 0 || array_text[i - 1] != '\\')) {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '{') {
            if (depth == 0) {
                object_start = i;
            }
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && object_start != std::string::npos) {
                objects.push_back(array_text.substr(object_start, i - object_start + 1));
                object_start = std::string::npos;
            }
        }
    }
    return objects;
}

bool extract_bodies_array(const std::string& text, std::string& out) {
    const std::size_t bodies_key = text.find("\"bodies\"");
    if (bodies_key == std::string::npos) {
        return false;
    }
    const std::size_t array_start = text.find('[', bodies_key);
    if (array_start == std::string::npos) {
        return false;
    }

    int depth = 0;
    bool in_string = false;
    for (std::size_t i = array_start; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '"' && (i == array_start || text[i - 1] != '\\')) {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                out = text.substr(array_start + 1, i - array_start - 1);
                return true;
            }
        }
    }
    return false;
}

} // namespace

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

void Renderer::begin_scene_save_dialog() {
    scene_io_mode_ = SceneIoMode::Save;
    scene_io_input_ = sanitize_filename_token(current_export_label_);
    scene_io_confirm_overwrite_ = false;
}

void Renderer::begin_scene_load_dialog() {
    refresh_saved_scene_files();
    scene_io_mode_ = SceneIoMode::Load;
    scene_io_confirm_delete_ = false;
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

void Renderer::refresh_saved_scene_files() {
    // Keep the import dialog deterministic by always presenting scene files in
    // sorted order and clamping the current selection.
    saved_scene_files_.clear();
    std::filesystem::create_directories(scene_directory_);
    for (const auto& entry : std::filesystem::directory_iterator(scene_directory_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".json") {
            saved_scene_files_.push_back(entry.path().filename().string());
        }
    }
    std::sort(saved_scene_files_.begin(), saved_scene_files_.end());
    if (saved_scene_files_.empty()) {
        selected_saved_scene_idx_ = 0;
    } else {
        selected_saved_scene_idx_ =
            std::clamp(selected_saved_scene_idx_, 0, static_cast<int>(saved_scene_files_.size()) - 1);
    }
}

bool Renderer::save_scene_json(const Simulator& sim, const std::string& scene_name) const {
    // Scene files capture enough metadata to rebuild the exact explorer state
    // later, not just the raw body list.
    std::filesystem::create_directories(scene_directory_);
    const std::string filename =
        (std::filesystem::path(scene_directory_) /
         (sanitize_filename_token(scene_name) + ".json")).string();

    std::ostringstream out;
    out << "{\n";
    out << "  \"name\": \"" << json_escape(scene_name) << "\",\n";
    out << "  \"gravity\": " << sim.get_gravity() << ",\n";
    out << "  \"dt\": " << (sim.get_dt() / std::max(1e-12, time_scale_)) << ",\n";
    out << "  \"integrator\": \"" << integrator_name(sim.get_integrator()) << "\",\n";
    out << "  \"substeps\": " << sim.get_substeps() << ",\n";
    out << "  \"meters_to_pixels\": " << scale_ << ",\n";
    out << "  \"bodies\": [\n";
    const auto& bodies = sim.get_bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        out << "    {\n";
        out << "      \"name\": \"" << json_escape(body.name) << "\",\n";
        out << "      \"mass\": " << body.mass << ",\n";
        out << "      \"radius\": " << body.radius << ",\n";
        out << "      \"color\": " << body.color << ",\n";
        out << "      \"is_satellite\": " << (body.is_satellite ? "true" : "false") << ",\n";
        out << "      \"is_star\": " << (body.is_star ? "true" : "false") << ",\n";
        out << "      \"pos_x\": " << body.pos.x << ",\n";
        out << "      \"pos_y\": " << body.pos.y << ",\n";
        out << "      \"vel_x\": " << body.vel.x << ",\n";
        out << "      \"vel_y\": " << body.vel.y << "\n";
        out << "    }";
        if (i + 1 < bodies.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << out.str();
    return static_cast<bool>(file);
}

bool Renderer::load_scene_json(const std::string& filename, ScenarioPreset& preset) const {
    std::ifstream in((std::filesystem::path(scene_directory_) / filename).string(), std::ios::binary);
    if (!in) {
        return false;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();

    if (!extract_string_field(text, "name", preset.name) ||
        !extract_double_field(text, "gravity", preset.gravity) ||
        !extract_double_field(text, "dt", preset.dt) ||
        !extract_int_field(text, "substeps", preset.substeps) ||
        !extract_double_field(text, "meters_to_pixels", preset.meters_to_pixels)) {
        return false;
    }

    std::string integrator_name_text;
    if (!extract_string_field(text, "integrator", integrator_name_text)) {
        return false;
    }
    preset.integrator =
        (integrator_name_text == "Euler") ? Integrator::Euler : Integrator::RK4;

    std::string bodies_array;
    if (!extract_bodies_array(text, bodies_array)) {
        return false;
    }

    // Reconstruct the preset body-by-body so the loaded scene can be passed
    // straight back into apply_scene without more translation.
    preset.bodies.clear();
    for (const std::string& object_text : split_top_level_objects(bodies_array)) {
        Body body;
        double pos_x = 0.0;
        double pos_y = 0.0;
        double vel_x = 0.0;
        double vel_y = 0.0;
        int color = 0;
        if (!extract_string_field(object_text, "name", body.name) ||
            !extract_double_field(object_text, "mass", body.mass) ||
            !extract_double_field(object_text, "radius", body.radius) ||
            !extract_int_field(object_text, "color", color) ||
            !extract_bool_field(object_text, "is_satellite", body.is_satellite) ||
            !extract_bool_field(object_text, "is_star", body.is_star) ||
            !extract_double_field(object_text, "pos_x", pos_x) ||
            !extract_double_field(object_text, "pos_y", pos_y) ||
            !extract_double_field(object_text, "vel_x", vel_x) ||
            !extract_double_field(object_text, "vel_y", vel_y)) {
            return false;
        }
        body.color = static_cast<std::uint32_t>(color);
        body.pos = Vec2{pos_x, pos_y};
        body.vel = Vec2{vel_x, vel_y};
        body.acc = Vec2{0.0, 0.0};
        preset.bodies.push_back(body);
    }

    return true;
}

} // namespace orbitsimlite
