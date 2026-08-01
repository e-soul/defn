// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_screen_scaffold.h"

#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/memory.hpp>

#include <string_view>

namespace defn {

using namespace godot;

namespace {

constexpr real_t FALLBACK_SCREEN_WIDTH = 900.0F;
constexpr real_t FALLBACK_SCREEN_HEIGHT = 600.0F;

godot::Vector2 content_size(Node *parent, const ScreenSpec &spec) {
    if (spec.max_content_size.x > 0.0F && spec.max_content_size.y > 0.0F) {
        return spec.max_content_size;
    }
    const UiScreenStyle &screen = UiThemeProvider::data().screen;
    godot::Vector2 viewport_size(FALLBACK_SCREEN_WIDTH, FALLBACK_SCREEN_HEIGHT);
    if (parent != nullptr && parent->get_viewport() != nullptr) {
        const godot::Vector2 visible = parent->get_viewport()->get_visible_rect().size;
        if (visible.x > 0.0F && visible.y > 0.0F) {
            viewport_size = visible;
        }
    }
    return {viewport_size.x * screen.content_max_width_ratio, viewport_size.y * screen.content_max_height_ratio};
}

} // namespace

UiScreenScaffold build_screen(Node *parent, const ScreenSpec &spec) {
    UiScreenScaffold scaffold;
    if (parent == nullptr) {
        return scaffold;
    }

    const UiScreenStyle &screen = UiThemeProvider::data().screen;
    const godot::Vector2 max_content_size = content_size(parent, spec);

    if (spec.show_backdrop) {
        auto *backdrop = memnew(ColorRect);
        backdrop->set_name("ScreenBackdrop");
        backdrop->set_color(UiThemeProvider::color(screen.backdrop_role));
        backdrop->set_mouse_filter(Control::MOUSE_FILTER_STOP);
        scaffold.root = backdrop;
    } else {
        auto *root = memnew(Control);
        root->set_name("ScreenRoot");
        root->set_mouse_filter(Control::MOUSE_FILTER_PASS);
        scaffold.root = root;
    }
    scaffold.root->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    // Keeps the chrome from collapsing when the root is laid out by a container parent instead of anchors.
    scaffold.root->set_custom_minimum_size(max_content_size);
    scaffold.root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    scaffold.root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    parent->add_child(scaffold.root);
    // Full-screen views can be mounted below CanvasLayer nodes or launched directly,
    // so they must not depend on a menu scene having installed the shared theme first.
    UiThemeProvider::apply_to(scaffold.root);

    auto *center = memnew(CenterContainer);
    center->set_name("ScreenCenter");
    center->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    scaffold.root->add_child(center);

    Control *content_host = center;
    if (spec.panelled_body) {
        auto *panel = make_surface(screen.panel_surface);
        panel->set_name("ScreenPanel");
        panel->set_custom_minimum_size({max_content_size.x, spec.constrain_height ? max_content_size.y : 0.0F});
        center->add_child(panel);
        scaffold.panel = panel;
        content_host = panel;
    }

    auto *column = memnew(VBoxContainer);
    column->set_name("ScreenColumn");
    column->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    column->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    column->add_theme_constant_override("separation", UiThemeProvider::spacing("section_gap"));
    content_host->add_child(column);

    auto *header = memnew(VBoxContainer);
    header->set_name("ScreenHeader");
    header->add_theme_constant_override("separation", UiThemeProvider::spacing("xs"));
    column->add_child(header);
    scaffold.header = header;

    if (!spec.title.is_empty()) {
        const std::string_view title_style =
            spec.title_text_style.empty() ? std::string_view(screen.title_text_style) : std::string_view(spec.title_text_style);
        auto *title = make_label(spec.title, title_style);
        title->set_name("ScreenTitle");
        title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        header->add_child(title);
    }
    if (!spec.subtitle.is_empty()) {
        auto *subtitle = make_label(spec.subtitle, screen.subtitle_text_style);
        subtitle->set_name("ScreenSubtitle");
        subtitle->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        header->add_child(subtitle);
    }

    scaffold.body = memnew(VBoxContainer);
    scaffold.body->set_name("ScreenBody");
    scaffold.body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    scaffold.body->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    scaffold.body->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));

    if (spec.scrollable_body) {
        auto *scroll = memnew(ScrollContainer);
        scroll->set_name("ScreenScroll");
        scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
        scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
        scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
        scroll->add_child(scaffold.body);
        column->add_child(scroll);
    } else {
        column->add_child(scaffold.body);
    }

    scaffold.footer = make_action_bar();
    scaffold.footer->set_name("ScreenFooter");
    column->add_child(scaffold.footer);

    return scaffold;
}

} // namespace defn
