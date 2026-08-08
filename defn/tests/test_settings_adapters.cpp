// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "settings_adapters.h"
#include "settings_use_case.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>

namespace defn {

namespace {

void remove_test_file(const godot::String &path) {
    if (godot::FileAccess::file_exists(path)) {
        godot::DirAccess::remove_absolute(path);
    }
}

} // namespace

DEFN_TEST(config_file_settings_store_round_trips_and_clamps_values) {
    const godot::String path = "user://defn_settings_adapters_test.cfg";
    remove_test_file(path);
    ConfigFileSettingsStore store(path);

    SettingsState state{
        .display_mode = 0,
        .resolution = {.width = 1024, .height = 576},
        .vsync_enabled = false,
        .bus_volumes = {{.bus_name = "Master", .volume_percent = 250.0}},
    };
    DEFN_REQUIRE(store.save(state));

    SettingsState defaults{.bus_volumes = {{.bus_name = "Master", .volume_percent = 50.0}}};
    const std::optional<SettingsState> loaded = store.load(defaults);
    DEFN_REQUIRE(loaded.has_value());
    DEFN_CHECK_EQ(loaded->display_mode, 0);
    DEFN_CHECK_EQ(loaded->resolution.width, 1024);
    DEFN_CHECK_EQ(loaded->resolution.height, 576);
    DEFN_CHECK(!loaded->vsync_enabled);
    DEFN_CHECK_EQ(SettingsUseCase::get_bus_volume_percent(*loaded, "Master"), 100.0);

    remove_test_file(path);
}

DEFN_TEST(config_file_settings_store_tolerates_missing_file) {
    const godot::String path = "user://defn_missing_settings_adapters_test.cfg";
    remove_test_file(path);
    ConfigFileSettingsStore store(path);
    const SettingsState defaults{
        .display_mode = 2,
        .resolution = {.width = 800, .height = 600},
        .vsync_enabled = true,
        .bus_volumes = {{.bus_name = "Master", .volume_percent = 33.0}},
    };

    DEFN_CHECK(!store.load(defaults).has_value());
}

} // namespace defn
