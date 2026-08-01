// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_widgets.h"

#include "ui_sfx_player.h"
#include "ui_theme_provider.h"

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/core/memory.hpp>

namespace defn {

using namespace godot;

namespace {

constexpr float DISABLED_MODULATE_ALPHA = 0.7F;
constexpr float DISABLED_MODULATE_VALUE = 0.5F;

} // namespace

void apply_label_style(Label *label, std::string_view text_style) {
    if (label != nullptr) {
        UiThemeProvider::apply_to(label);
        label->set_theme_type_variation(UiThemeProvider::label_variation(text_style));
    }
}

void apply_button_style(Control *control, std::string_view variant) {
    if (control != nullptr) {
        UiThemeProvider::apply_to(control);
        control->set_theme_type_variation(UiThemeProvider::button_variation(variant));
    }
}

Label *make_label(const String &text, std::string_view text_style) {
    auto *label = memnew(Label);
    label->set_text(text);
    label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    apply_label_style(label, text_style);
    return label;
}

Button *make_button(const String &text, std::string_view variant, const Callable &pressed, UiSfxPlayer *sfx) {
    auto *button = memnew(Button);
    button->set_text(text);
    button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_style(button, variant);

    const UiButtonVariant *button_style = UiThemeProvider::data().find_button(variant);
    if (button_style != nullptr && (button_style->min_width > 0 || button_style->min_height > 0)) {
        button->set_custom_minimum_size({static_cast<real_t>(button_style->min_width), static_cast<real_t>(button_style->min_height)});
    }

    connect_sfx(sfx, button);
    if (pressed.is_valid()) {
        button->connect("pressed", pressed);
    }
    return button;
}

PanelContainer *make_surface(std::string_view surface_name) {
    auto *panel = memnew(PanelContainer);
    UiThemeProvider::apply_to(panel);
    panel->set_theme_type_variation(UiThemeProvider::panel_variation(surface_name));
    return panel;
}

PanelContainer *make_chip(const String &text, std::string_view color_role) {
    auto *chip = make_surface("chip");
    auto *label = make_label(text, "secondary");
    label->add_theme_color_override("font_color", UiThemeProvider::color(color_role));
    chip->add_child(label);
    return chip;
}

HBoxContainer *make_stat_row(const String &label, const String &value) {
    auto *row = memnew(HBoxContainer);
    row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

    auto *label_control = make_label(label, "score_stat");
    label_control->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    row->add_child(label_control);

    auto *value_control = make_label(value, "score_value");
    value_control->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    row->add_child(value_control);

    return row;
}

VBoxContainer *make_stat_cell(const String &heading, Label *&value_out) {
    auto *cell = memnew(VBoxContainer);
    cell->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    cell->add_theme_constant_override("separation", UiThemeProvider::spacing("xs"));

    auto *heading_label = make_label(heading, "stat_name");
    heading_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    cell->add_child(heading_label);

    value_out = make_label({}, "stat_value");
    value_out->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    cell->add_child(value_out);

    return cell;
}

HBoxContainer *make_action_bar() {
    auto *bar = memnew(HBoxContainer);
    bar->set_alignment(BoxContainer::ALIGNMENT_END);
    bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    bar->add_theme_constant_override("separation", UiThemeProvider::spacing(UiThemeProvider::data().screen.footer_gap_role));
    return bar;
}

Control *make_spacer(float height) {
    auto *spacer = memnew(Control);
    spacer->set_custom_minimum_size({0.0F, height});
    spacer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    return spacer;
}

void set_state_tint(Control *control, std::string_view color_role) {
    if (control != nullptr) {
        control->add_theme_color_override("font_color", UiThemeProvider::color(color_role));
    }
}

void apply_enabled(BaseButton *button, bool enabled) {
    if (button != nullptr) {
        button->set_disabled(!enabled);
        button->set_modulate(enabled ? godot::Color(1, 1, 1, 1)
                                     : godot::Color(DISABLED_MODULATE_VALUE, DISABLED_MODULATE_VALUE, DISABLED_MODULATE_VALUE, DISABLED_MODULATE_ALPHA));
    }
}

void connect_sfx(UiSfxPlayer *sfx, BaseButton *button) {
    if (sfx != nullptr) {
        sfx->connect_menu_button(button);
    }
}

} // namespace defn
