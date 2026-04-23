// OrbitSimLite - Renderer import/export implementation
#include "renderer.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace orbitsimlite {

namespace {

// CSV export is intended for spreadsheets and external plotting tools, so
// always quote names to avoid subtle delimiter issues.
std::string csv_escape(const std::string& value) {
    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped += ch;
        }
    }
    escaped += "\"";
    return escaped;
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

std::string format_datetime_token() {
    // Timestamp once per run so headless export rewrites a stable filename
    // instead of creating a new file every simulation step.
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm {};
#if defined(_WIN32)
    localtime_s(&local_tm, &now_time);
#else
    localtime_r(&now_time, &local_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

bool write_text_atomically(const std::string& filename, const std::string& content) {
    // Write to a temporary file and replace the previous file only when the
    // new snapshot is complete. Readers should never observe a half-written
    // or temporarily empty export file.
    const std::filesystem::path target(filename);
    const std::filesystem::path temp = target.string() + ".tmp";

    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << content;
        out.flush();
        if (!out) {
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (!ec) {
        return true;
    }

    std::filesystem::remove(target, ec);
    ec.clear();
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
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
    // The scene loader uses a lightweight parser because the exported scene
    // format is tightly controlled by this project.
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

void Renderer::refresh_export_targets() {
    // Bind export filenames to the current scene label once so later writes can
    // simply overwrite the same JSON/CSV snapshots in place.
    const std::string base_name =
        "bodies_" + format_datetime_token() + "_" + sanitize_filename_token(current_export_label_);
    current_json_export_path_ =
        (std::filesystem::path(export_directory_) / (base_name + ".json")).string();
    current_csv_export_path_ =
        (std::filesystem::path(export_directory_) / (base_name + ".csv")).string();
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
    return write_text_atomically(filename, out.str());
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

void Renderer::export_simulation_data(const Simulator& sim) {
    if (!output_options_.enable_json && !output_options_.enable_csv) {
        return;
    }

    std::filesystem::create_directories(export_directory_);

    // JSON and CSV are independent so users can choose one, the other, or both
    // from the terminal launcher without affecting the live simulation.
    if (output_options_.enable_json) {
        write_state_json(sim, current_json_export_path_);
    }
    if (output_options_.enable_csv) {
        write_state_csv(sim, current_csv_export_path_);
    }
}

void Renderer::write_state_json(const Simulator& sim, const std::string& filename) const {
    // Snapshot exports intentionally mirror the live explorer state closely so
    // external tools can reuse the same semantics shown in the UI.
    std::ostringstream out;
    const auto& bodies = sim.get_bodies();
    out << "{\n";
    out << "  \"simulation_name\": \"" << current_export_label_ << "\",\n";
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

    write_text_atomically(filename, out.str());
}

void Renderer::write_state_csv(const Simulator& sim, const std::string& filename) const {
    // CSV stays flat on purpose: one body per row, one snapshot per file write.
    std::ostringstream out;
    out << "time_seconds,body_index,name,mass,radius,color,is_satellite,is_star,pos_x,pos_y,vel_x,vel_y,acc_x,acc_y\n";
    const auto& bodies = sim.get_bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const auto& body = bodies[i];
        const std::string name = body.name.empty() ? ("body_" + std::to_string(i)) : body.name;
        out << sim.get_time() << ','
            << i << ','
            << csv_escape(name) << ','
            << body.mass << ','
            << body.radius << ','
            << body.color << ','
            << (body.is_satellite ? 1 : 0) << ','
            << (body.is_star ? 1 : 0) << ','
            << body.pos.x << ','
            << body.pos.y << ','
            << body.vel.x << ','
            << body.vel.y << ','
            << body.acc.x << ','
            << body.acc.y << '\n';
    }

    write_text_atomically(filename, out.str());
}

} // namespace orbitsimlite
