#ifndef SETTINGS_MODELS_H
#define SETTINGS_MODELS_H

#include <string>
#include <vector>

namespace defn {

struct SettingsResolution {
    int width = 1920;
    int height = 1080;

    bool operator==(const SettingsResolution &) const = default;
};

struct AudioBusSetting {
    std::string bus_name;
    double volume_percent = 100.0;
};

struct SettingsDisplayState {
    int display_mode = 0;
    SettingsResolution resolution;
    bool vsync_enabled = true;
};

struct SettingsState {
    int display_mode = 0;
    SettingsResolution resolution;
    bool vsync_enabled = true;
    std::vector<AudioBusSetting> bus_volumes;
};

} // namespace defn

#endif