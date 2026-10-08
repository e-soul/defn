// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_SCREEN_SCAFFOLD_H
#define UI_SCREEN_SCAFFOLD_H
#include "ui_widgets.h"
#include <godot_cpp/classes/scroll_container.hpp>

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_flow_container.hpp>
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
    /// Lets the panel shrink around its content instead of filling the content box. Menus and dialogs carry a
    /// handful of controls and look wrong stretched to the width a data-heavy screen wants.
    bool fit_content = false;
    /// Simple button menus can tighten their chrome on short phone landscapes before requiring scrolling.
    bool compact_menu = false;
    /// Positive axes cap the panel after the local usable bounds and screen margins are applied.
    godot::Vector2 content_limit;
    /// Shared backdrop and panel, with a plain content host for measured, paged layouts.
    bool manual_content = false;
};

class UiScreenControl : public UiContextControl {
    GDCLASS(UiScreenControl, UiContextControl)
  public:
    void _process(double delta) override;
    void request_layout();
    void refresh_menu_chrome(const UiThemeData &theme);
    void refresh_layout();
    ScreenSpec spec;
    godot::PanelContainer *panel = nullptr;
    godot::VBoxContainer *column = nullptr;
    godot::VBoxContainer *body = nullptr;
    godot::Label *title = nullptr;
    godot::ScrollContainer *scroll = nullptr;
    godot::Control *footer = nullptr;

  protected:
    static void _bind_methods() {}
    void context_changed() override { request_layout(); }
    void _notification(int what);

  private:
    bool dirty_ = true;
};

struct UiScreenScaffold {
    godot::Control *root = nullptr;
    godot::PanelContainer *panel = nullptr;
    godot::Control *header = nullptr;
    godot::VBoxContainer *body = nullptr;
    godot::HFlowContainer *footer = nullptr;
    godot::Control *content = nullptr;
};

/// Builds the shared backdrop / header / body / footer chrome used by every full-screen view.
UiScreenScaffold build_screen(godot::Node *parent, const ScreenSpec &spec, UiScreenControl *controller = nullptr);

} // namespace defn

#endif
