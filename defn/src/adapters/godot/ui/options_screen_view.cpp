// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "options_screen_view.h"
#include "ui_theme_provider.h"
#include <godot_cpp/classes/style_box_flat.hpp>

namespace defn {
using namespace godot;

void OptionsScreenView::add_desktop_control(Control *control) {
    body->add_child(control);
    desktop_controls_.push_back(control);
    reflow_options();
}

void OptionsScreenView::add_volume_control(const VolumeOptionControls &controls) {
    auto *volume_column = memnew(VBoxContainer);
    volume_column->set_name("VolumeOption");
    volume_column->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    volume_column->add_child(controls.row);
    body->add_child(volume_column);
    volume_rows_.push_back({.column = volume_column, .controls = controls});
    reflow_options();
}

void OptionsScreenView::context_changed() { reflow_options(); }

void OptionsScreenView::reflow_options() {
    if (panel == nullptr || column == nullptr) {
        return;
    }
    const bool audio_only = uses_mobile_menu_layout(context_.display);
    for (auto *control : desktop_controls_) {
        control->set_visible(!audio_only);
    }
    for (const auto &volume : volume_rows_) {
        reflow_volume(volume, audio_only);
    }
    // Options may use the available height; simple button menus keep their smaller natural-height cap.
    spec.content_limit.y = UiThemeProvider::metric("options_content_height", 800);
    const Ref<StyleBoxFlat> style = UiThemeProvider::surface("panel")->duplicate();
    const auto padding = static_cast<float>(UiThemeProvider::spacing(audio_only ? "sm" : "xl"));
    for (const auto side : {SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM}) {
        style->set_content_margin(side, padding);
    }
    panel->add_theme_stylebox_override("panel", style);
    body->add_theme_constant_override("separation", UiThemeProvider::spacing("md"));
    column->add_theme_constant_override("separation", UiThemeProvider::spacing(audio_only ? "sm" : "section_gap"));
    request_layout();
}

void OptionsScreenView::reflow_volume(const VolumeRow &volume, bool audio_only) {
    const auto &controls = volume.controls;
    Node *name_parent = audio_only ? static_cast<Node *>(volume.column) : static_cast<Node *>(controls.row);
    if (controls.name->get_parent() != name_parent) {
        controls.name->reparent(name_parent, false);
        name_parent->move_child(controls.name, 0);
    }
    controls.name->set_horizontal_alignment(audio_only ? HORIZONTAL_ALIGNMENT_LEFT : HORIZONTAL_ALIGNMENT_RIGHT);
    controls.name->set_custom_minimum_size({audio_only ? 0.0F : UiThemeProvider::metric("option_label_width"), 0});
    const auto *variant = UiThemeProvider::data().find_button("option_control");
    const auto desktop_width = static_cast<float>(variant == nullptr ? 160 : variant->min_width);
    const float width = audio_only ? 0.0F : desktop_width;
    const auto height = static_cast<float>(UiThemeProvider::data().responsive.touch_target);
    controls.slider->set_custom_minimum_size({width, height});
    controls.slider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
}

} // namespace defn
