// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string.hpp>

#include <string_view>

namespace defn {

class UiSfxPlayer;

/// Shared control factories; every visual value comes from `UiThemeProvider`.
godot::Label *make_label(const godot::String &text, std::string_view text_style = "body");
godot::Button *make_button(const godot::String &text, std::string_view variant = "secondary", const godot::Callable &pressed = {},
                           UiSfxPlayer *sfx = nullptr);
godot::PanelContainer *make_surface(std::string_view surface_name = "panel");
godot::PanelContainer *make_chip(const godot::String &text, std::string_view color_role = "text_secondary");
godot::HBoxContainer *make_stat_row(const godot::String &label, const godot::String &value);
godot::VBoxContainer *make_stat_cell(const godot::String &heading, godot::Label *&value_out);
godot::HBoxContainer *make_action_bar();
godot::Control *make_spacer(float height);

void apply_label_style(godot::Label *label, std::string_view text_style);
void apply_button_style(godot::Control *control, std::string_view variant);
void set_state_tint(godot::Control *control, std::string_view color_role);
void apply_enabled(godot::BaseButton *button, bool enabled);
void connect_sfx(UiSfxPlayer *sfx, godot::BaseButton *button);

} // namespace defn

#endif
