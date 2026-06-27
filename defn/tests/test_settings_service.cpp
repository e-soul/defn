#include "test_harness.h"

#include "settings_service.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>

namespace defn {

namespace {

void remove_test_file(const String &path) {
    if (FileAccess::file_exists(path)) {
        DirAccess::remove_absolute(path);
    }
}

} // namespace

DEFN_TEST(settings_service_parses_resolution_values_with_fallbacks) {
    DEFN_CHECK_EQ(SettingsService::parse_resolution_value("1280x720"), Vector2i(1280, 720));
    DEFN_CHECK_EQ(SettingsService::parse_resolution_value("1920", Vector2i(800, 600)), Vector2i(800, 600));
    DEFN_CHECK_EQ(SettingsService::parse_resolution_value("badxvalue", Vector2i(640, 360)), Vector2i(0, 0));
}

DEFN_TEST(settings_service_clamps_and_retrieves_bus_volume_state) {
    SettingsState state;

    DEFN_CHECK_EQ(SettingsService::get_bus_volume_percent(state, "Master", 77.0), 77.0);

    SettingsService::set_bus_volume_percent(state, "Master", 150.0);
    DEFN_CHECK_EQ(state.bus_volumes.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(SettingsService::get_bus_volume_percent(state, "Master"), 100.0);

    SettingsService::set_bus_volume_percent(state, "Master", -10.0);
    DEFN_CHECK_EQ(state.bus_volumes.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(SettingsService::get_bus_volume_percent(state, "Master"), 0.0);

    SettingsService::set_bus_volume_percent(state, "Effects", 45.0);
    DEFN_CHECK_EQ(SettingsService::get_bus_volume_percent(state, "Effects"), 45.0);
}

DEFN_TEST(settings_service_saves_and_loads_video_and_existing_audio_settings) {
    const String path = "user://defn_settings_service_test.cfg";
    remove_test_file(path);

    SettingsState state;
    state.display_mode = DisplayServer::WINDOW_MODE_WINDOWED;
    state.resolution = Vector2i(1024, 576);
    state.vsync_enabled = false;
    state.bus_volumes.push_back({.bus_name = "Master", .volume_percent = 250.0});

    DEFN_REQUIRE(SettingsService::save(state, path));

    const SettingsState loaded = SettingsService::load_or_default(path);
    DEFN_CHECK_EQ(loaded.display_mode, DisplayServer::WINDOW_MODE_WINDOWED);
    DEFN_CHECK_EQ(loaded.resolution, Vector2i(1024, 576));
    DEFN_CHECK(!loaded.vsync_enabled);
    DEFN_CHECK_EQ(SettingsService::get_bus_volume_percent(loaded, "Master"), 100.0);

    remove_test_file(path);
}

DEFN_TEST(settings_service_load_or_default_tolerates_missing_file) {
    const String path = "user://defn_missing_settings_service_test.cfg";
    remove_test_file(path);

    const SettingsState state = SettingsService::load_or_default(path);
    DEFN_CHECK(state.resolution.x >= 0);
    DEFN_CHECK(state.resolution.y >= 0);
    DEFN_CHECK_EQ(SettingsService::get_bus_volume_percent(state, "Missing", 33.0), 33.0);
}

} // namespace defn