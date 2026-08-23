// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_THEME_PROVIDER_H
#define UI_THEME_PROVIDER_H

#include "ui_theme_models.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <string_view>

namespace defn {

/// Single source of look-and-feel: parses `res://data/ui_theme.json` once and turns it into a shared `godot::Theme`.
class UiThemeProvider {
  public:
    UiThemeProvider() = delete;

    static const UiThemeData &data();
    static godot::Ref<godot::Theme> theme();
    static void install(godot::SceneTree *tree);
    static void apply_to(godot::Control *control);
    static void reload();

    static godot::Color color(std::string_view role);
    /// A named layout figure as the `real_t` Godot sizing calls want. `UiThemeData::metric` still serves the few
    /// places that need the raw integer, such as counts and theme constants.
    static godot::real_t metric(std::string_view name, int fallback = 0);
    static int font_size(std::string_view role);
    static int spacing(std::string_view role);
    static int shape(std::string_view role);
    static godot::Ref<godot::StyleBoxFlat> surface(std::string_view name);

    /// JSON entry key (`screen_title`) to the registered theme type variation (`ScreenTitleLabel`).
    static godot::StringName label_variation(std::string_view text_style_name);
    static godot::StringName button_variation(std::string_view button_name);
    static godot::StringName panel_variation(std::string_view surface_name);
};

} // namespace defn

#endif
