// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DATA_PATHS_H
#define DATA_PATHS_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn::DataPaths {

using namespace godot;

inline constexpr auto LEVELS_DIRECTORY = "res://data/levels";
inline constexpr auto CAMPAIGN_MAP_DATA = "res://data/campaign_map.json";
inline constexpr auto MENU_DATA = "res://data/menu_data.json";
inline constexpr auto UI_THEME = "res://data/ui_theme.json";
inline constexpr auto MUSIC_PLAYLIST_DATA = "res://data/music_playlist.json";
inline constexpr auto PROGRESSION_DATA = "res://data/progression.json";
inline constexpr auto UPGRADES_DATA = "res://data/upgrades.json";
inline constexpr auto UNIT_DATA = "res://data/unit_data.json";
inline constexpr auto UNIT_GLOBALS = "res://data/unit_globals.json";
inline constexpr auto UNIT_CONTROL = "res://data/unit_control.json";
inline constexpr auto SAVE_DATA = "user://save_data.json";

inline String level_definition(const String &level_id) { return vformat("%s/%s.json", String(LEVELS_DIRECTORY), level_id); }

} // namespace defn::DataPaths

#endif
