// OrbitSimLite - SFML-based renderer
//
// Responsible for visualising the current Simulator state:
//  - fixed world-to-screen mapping (metres -> pixels)
//  - drawing bodies as circles with fading trails
//  - interactive controls for presets, editing, and time scaling
//  - live numerical panel and current-state JSON export
#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "simulator.hpp"
#include "utils.hpp"

namespace orbitsimlite {

enum class FieldMode {
    // Draw nothing beyond the bodies and their trails.
    None,
    // Sample the gravitational field and display local arrow directions.
    Arrows,
    // Warp a square grid to provide an intuitive field-curvature view.
    Grid,
};

enum class CollisionMode {
    // Pause and ask the user before removing the lighter body.
    PromptRemoveSmaller,
    // Remove the lighter body immediately.
    RemoveSmaller,
    // Replace two colliding bodies with a merged equivalent body.
    Merge,
    // Keep both bodies and ignore the overlap visually.
    Ignore,
};

struct ScenarioPreset {
    // Human-readable scene label used in the UI and export names.
    std::string name;
    // Full initial body state for the preset.
    std::vector<Body> bodies;
    // Physics and rendering parameters that make the preset reproducible.
    double gravity {Physics::DefaultG};
    double dt {1.0};
    Integrator integrator {Integrator::RK4};
    int substeps {1};
    double meters_to_pixels {2e-9};
};

struct OutputOptions {
    // Enable machine-readable snapshot exports alongside the visual app.
    bool enable_json {true};
    bool enable_csv {false};
};

class Renderer {
public:
    Renderer(unsigned width = 1000, unsigned height = 800, double meters_to_pixels = 2e-9);

    // Optional preset list used by the interactive explorer. When set, reset
    // restores the active preset and headless mode can pick from the list.
    void set_presets(const std::vector<ScenarioPreset>& presets);
    void set_output_options(const OutputOptions& options);

    // Runs the visualization loop. Blocks until window close.
    void run(Simulator& sim);
    void run_headless(Simulator& sim, std::optional<double> real_time_seconds, const std::string& preset_name = "Headless Scene");

private:
    enum class SceneIoMode {
        // No scene dialog is currently visible.
        None,
        // The export dialog is collecting a target scene name.
        Save,
        // The import dialog is browsing saved scene files.
        Load,
    };

    // Coordinate conversion and shared renderer bookkeeping.
    sf::Vector2f world_to_screen(const Vec2& p) const;
    Vec2 screen_to_world(const sf::Vector2f& p) const;
    void rebuild_trails(std::size_t count);
    // Data export helpers.
    void write_state_json(const Simulator& sim, const std::string& filename) const;
    void write_state_csv(const Simulator& sim, const std::string& filename) const;
    void ensure_font_loaded();
    void refresh_export_targets();
    void export_simulation_data(const Simulator& sim);
    void update_window_title(sf::RenderWindow& window, const Simulator& sim, const std::string& preset_name) const;
    // Scene/preset management and interactive input handling.
    void sync_selected_index(const Simulator& sim);
    void apply_scene(Simulator& sim, const ScenarioPreset& scene);
    void apply_preset(Simulator& sim, std::size_t idx);
    void begin_scene_save_dialog();
    void begin_scene_load_dialog();
    void handle_mouse_move(const sf::Vector2i& position);
    void handle_left_click(const sf::Vector2f& mouse_pos, const Simulator& sim);
    void begin_view_drag(const sf::Vector2i& position);
    void end_view_drag();
    void handle_text_input(std::uint32_t unicode);
    bool handle_scene_io_key(sf::Keyboard::Key key, Simulator& sim);
    void handle_body_creation(Simulator& sim);
    void handle_selected_body_removal(Simulator& sim);
    void handle_selected_body_edit(sf::Keyboard::Key key, Simulator& sim);
    void handle_main_key(sf::Keyboard::Key key, Simulator& sim, bool& request_single_step);
    // Drawing helpers and UI surfaces.
    void draw_scene_io_overlay(sf::RenderWindow& window) const;
    void refresh_saved_scene_files();
    bool save_scene_json(const Simulator& sim, const std::string& scene_name) const;
    bool load_scene_json(const std::string& filename, ScenarioPreset& preset) const;
    void draw_field_overlay(sf::RenderWindow& window, const Simulator& sim) const;
    void draw_arrow_field_overlay(sf::RenderWindow& window, const Simulator& sim) const;
    void draw_grid_field_overlay(sf::RenderWindow& window, const Simulator& sim) const;
    void draw_body_trails(sf::RenderWindow& window, const Simulator& sim);
    void draw_bodies(sf::RenderWindow& window, const Simulator& sim) const;
    void draw_body_vectors(sf::RenderWindow& window, const Simulator& sim) const;
    void draw_prediction_path(sf::RenderWindow& window, const Simulator& sim) const;
    void draw_side_panel(sf::RenderWindow& window, const Simulator& sim, const std::string& preset_name) const;
    void draw_body_highlight(sf::RenderWindow& window, const Body& body) const;
    void draw_editor_cursor(sf::RenderWindow& window, const Vec2& world_point) const;
    void draw_arrow(sf::RenderWindow& window, const sf::Vector2f& start, const Vec2& world_vec, const sf::Color& color, float scale, float max_length) const;
    // Collision lifecycle helpers used by the interactive loop.
    void confirm_collision_prompt(Simulator& sim);
    void resolve_scene_collisions(Simulator& sim);
    void handle_collision_resolution(Simulator& sim, std::size_t idx_a, std::size_t idx_b);
    void remove_bodies_by_index(Simulator& sim, const std::vector<std::size_t>& to_remove);
    std::string format_scientific(double value, int precision = 3) const;
    std::string collision_mode_name() const;
    std::string field_mode_name() const;
    std::string integrator_name(Integrator integrator) const;

    // Window and camera state.
    unsigned width_;
    unsigned height_;
    double scale_; // metres to pixels
    bool paused_ {false};
    FieldMode field_mode_ {FieldMode::Arrows};
    bool show_vectors_ {true};
    bool show_prediction_ {true};
    bool font_loaded_ {false};
    bool dragging_view_ {false};
    int selected_body_idx_ {-1};
    double time_scale_ {1.0};
    double zoom_ {1.0};
    std::size_t custom_body_counter_ {1};
    sf::Vector2i last_mouse_pos_ {0, 0};
    Vec2 camera_center_ {};
    Vec2 cursor_world_ {};
    const std::size_t max_trail_ = 200;
    const float side_panel_width_ = 320.0f;

    // Persistent render/cache state.
    std::vector<std::deque<Vec2>> trails_;
    std::vector<ScenarioPreset> presets_;
    sf::Font ui_font_;
    CollisionMode collision_mode_ {CollisionMode::PromptRemoveSmaller};
    // Scene import/export overlay state.
    SceneIoMode scene_io_mode_ {SceneIoMode::None};
    std::string scene_io_input_;
    bool scene_io_confirm_overwrite_ {false};
    bool scene_io_confirm_delete_ {false};
    std::vector<std::string> saved_scene_files_;
    int selected_saved_scene_idx_ {0};
    std::string scene_io_message_;

    // Collision prompt state when interactive confirmation is required.
    bool collision_active_ {false};
    std::size_t collision_idx_keep_ {0};
    std::size_t collision_idx_remove_ {0};

    // Snapshot export and current-scene metadata.
    OutputOptions output_options_ {};
    std::string export_directory_ {"ExportData"};
    std::string scene_directory_ {"Exports"};
    std::string current_export_label_ {"Scene"};
    std::string current_preset_name_ {"Scene"};
    std::size_t current_preset_idx_ {0};
    std::string current_json_export_path_;
    std::string current_csv_export_path_;
    ScenarioPreset current_scene_ {};
};

} // namespace orbitsimlite
