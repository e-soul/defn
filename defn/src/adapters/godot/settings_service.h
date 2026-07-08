#ifndef SETTINGS_SERVICE_H
#define SETTINGS_SERVICE_H

#include "settings_models.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace defn {

using namespace godot;

inline constexpr const char *SETTINGS_PATH = "user://settings.cfg";

class SettingsService {
  public:
    SettingsService() = delete;

    static SettingsState load_or_default(const String &path = SETTINGS_PATH);
    static bool save(const SettingsState &state, const String &path = SETTINGS_PATH);
    static void apply(const SettingsState &state);

    static void set_display_mode(SettingsState &state, DisplayServer::WindowMode mode);
    static void set_resolution(SettingsState &state, const Vector2i &resolution);
    static void set_vsync(SettingsState &state, bool enabled);
    static void set_bus_volume_percent(SettingsState &state, const String &bus_name, double volume_percent);
    static double get_bus_volume_percent(const SettingsState &state, const String &bus_name, double fallback = 100.0);
    static Vector2i parse_resolution_value(const String &value_str, const Vector2i &fallback = Vector2i(1920, 1080));
};

} // namespace defn

#endif