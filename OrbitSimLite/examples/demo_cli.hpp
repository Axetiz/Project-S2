// OrbitSimLite - shared terminal launch helpers for demos
#pragma once

#include <cctype>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

namespace orbitsimlite_demo {

struct LaunchOptions {
    // Output backends can be combined so one run can drive both the GUI and
    // machine-readable export files.
    bool enable_sfml {true};
    bool enable_json {true};
    bool enable_csv {false};
    // Headless runs stop after a wall-clock duration unless INF is requested.
    std::optional<double> headless_real_time_seconds {10.0};
    std::size_t headless_preset_idx {0};
};

inline std::string normalize_modes(std::string modes) {
    // Accept flexible user input such as \"SJ\" or \"s j\" and normalize it to
    // the minimal lowercase token set used by the launcher.
    std::string normalized;
    for (char ch : modes) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }
    return normalized;
}

inline LaunchOptions read_launch_options(std::size_t preset_count = 1) {
    LaunchOptions options;

    // Let the user choose all active output backends up front so the demos and
    // headless exporter share a single consistent startup flow.
    std::string modes;
    std::cout << "Choose output modes [s=SFML, j=JSON, c=CSV].\n";
    std::cout << "You can combine them, for example: sj, sc, jc, sjc\n";
    std::cout << "Enter modes (default: sj): ";
    std::getline(std::cin >> std::ws, modes);
    modes = normalize_modes(modes);
    if (modes.empty()) {
        modes = "sj";
    }

    options.enable_sfml = modes.find('s') != std::string::npos;
    options.enable_json = modes.find('j') != std::string::npos;
    options.enable_csv = modes.find('c') != std::string::npos;

    if (!options.enable_sfml && !options.enable_json && !options.enable_csv) {
        options.enable_sfml = true;
        options.enable_json = true;
    }

    if (!options.enable_sfml) {
        if (preset_count > 1) {
            std::size_t preset_choice = 1;
            std::cout << "Headless mode selected.\n";
            std::cout << "Choose preset to simulate (1-" << preset_count << ", default 1): ";
            std::cin >> preset_choice;
            if (!std::cin || preset_choice < 1 || preset_choice > preset_count) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                preset_choice = 1;
            }
            options.headless_preset_idx = preset_choice - 1;
        } else {
            std::cout << "Headless mode selected.\n";
        }

        std::string duration_text;
        std::cout << "Enter real-time duration in seconds for export (default 10, INF for endless): ";
        std::getline(std::cin >> std::ws, duration_text);
        duration_text = normalize_modes(duration_text) == "inf" ? "INF" : duration_text;
        if (duration_text.empty()) {
            options.headless_real_time_seconds = 10.0;
        } else if (duration_text == "INF") {
            options.headless_real_time_seconds.reset();
        } else {
            try {
                const double seconds = std::stod(duration_text);
                options.headless_real_time_seconds = seconds > 0.0 ? std::optional<double>{seconds}
                                                                   : std::optional<double>{10.0};
            } catch (...) {
                options.headless_real_time_seconds = 10.0;
            }
        }
    }

    return options;
}

} // namespace orbitsimlite_demo
