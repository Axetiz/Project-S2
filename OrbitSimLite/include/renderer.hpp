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
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "simulator.hpp"
#include "utils.hpp"

namespace orbitsimlite {

enum class FieldMode {
    None,
    Arrows,
    Grid,
};

enum class CollisionMode {
    PromptRemoveSmaller,
    RemoveSmaller,
    Merge,
    Ignore,
};

struct HistoryFrame {
    double time_seconds {0.0};
    std::vector<Body> bodies;
};

struct ScenarioPreset {
    std::string name;
    std::vector<Body> bodies;
    double gravity {Physics::DefaultG};
    double dt {1.0};
    Integrator integrator {Integrator::RK4};
    int substeps {1};
    double meters_to_pixels {2e-9};
};

class Renderer {
public:
    Renderer(unsigned width = 1000, unsigned height = 800, double meters_to_pixels = 2e-9);

    // Optional preset list used by the interactive explorer. When set, keys
    // 1-9 switch between scenarios and reset restores the current preset.
    void set_presets(const std::vector<ScenarioPreset>& presets);

    // Runs the visualization loop. Blocks until window close.
    void run(Simulator& sim);

private:
    sf::Vector2f world_to_screen(const Vec2& p) const;
    Vec2 screen_to_world(const sf::Vector2f& p) const;
    void rebuild_trails(std::size_t count);
    void write_state_json(const Simulator& sim, const std::string& filename) const;
    void record_history_frame(const Simulator& sim);
    void write_history_json(const std::string& filename) const;
    void ensure_font_loaded();
    void export_simulation_data(const Simulator& sim);
    void update_window_title(sf::RenderWindow& window, const Simulator& sim, const std::string& preset_name) const;
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
    void handle_collision_resolution(Simulator& sim, std::size_t idx_a, std::size_t idx_b);
    void remove_bodies_by_index(Simulator& sim, const std::vector<std::size_t>& to_remove);
    std::string format_scientific(double value, int precision = 3) const;
    std::string collision_mode_name() const;
    std::string field_mode_name() const;
    std::string integrator_name(Integrator integrator) const;

    unsigned width_;
    unsigned height_;
    double scale_; // meters to pixels
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

    std::vector<std::deque<Vec2>> trails_;
    std::deque<HistoryFrame> history_frames_;
    std::vector<ScenarioPreset> presets_;
    sf::Font ui_font_;
    CollisionMode collision_mode_ {CollisionMode::PromptRemoveSmaller};

    // Collision handling state
    bool collision_active_ {false};
    std::size_t collision_idx_keep_ {0};
    std::size_t collision_idx_remove_ {0};

    // JSON state output file (overwritten each frame)
    std::string state_filename_ {"bodies.json"};
    std::string history_filename_ {"bodies_history.json"};
    std::size_t max_history_frames_ {180};
};

} // namespace orbitsimlite
