// OrbitSimLite - Renderer live export implementation
#include "renderer.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
