// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_SCREEN_SCAFFOLD_H
#define UI_SCREEN_SCAFFOLD_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <string>

namespace defn {

struct ScreenSpec {
    godot::String title;
    godot::String subtitle;
    /// Overrides the theme's screen title style when non-empty.
    std::string title_text_style;
    bool show_backdrop = true;
    bool panelled_body = true;
    bool scrollable_body = true;
    /// Keeps the panel inside max_content_size and lets a scrollable body absorb overflow.
    bool constrain_height = false;
    /// Overrides the viewport-derived content box when both axes are positive.
    godot::Vector2 max_content_size;
};

struct UiScreenScaffold {
    godot::Control *root = nullptr;
    godot::PanelContainer *panel = nullptr;
    godot::Control *header = nullptr;
    godot::VBoxContainer *body = nullptr;
    godot::HBoxContainer *footer = nullptr;
};

/// Builds the shared backdrop / header / body / footer chrome used by every full-screen view.
UiScreenScaffold build_screen(godot::Node *parent, const ScreenSpec &spec);

} // namespace defn

#endif
