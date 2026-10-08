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
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <string_view>

namespace defn {

using namespace godot;

void UiScreenControl::request_layout() { dirty_ = true; }
void UiScreenControl::_notification(int what) {
    if (what == NOTIFICATION_RESIZED || what == NOTIFICATION_THEME_CHANGED) {
        request_layout();
    }
}
void UiScreenControl::_process(double /*delta*/) {
    if (dirty_) {
        refresh_layout();
    }
}
void UiScreenControl::refresh_layout() {
    dirty_ = false;
    if (spec.manual_content || panel == nullptr || column == nullptr) {
        return;
    }
    const auto &theme = UiThemeProvider::data();
    const auto margin = static_cast<float>(theme.spacing.screen_margin);
    const float preferred_width =
        spec.fit_content ? static_cast<float>(theme.metric("menu_content_width", 320)) : static_cast<float>(theme.metric("screen_content_width", 1000));
    const float preferred_height = spec.fit_content ? static_cast<float>(theme.metric("menu_content_height", 360)) : get_size().y;
    const float width = std::min(spec.content_limit.x > 0 ? spec.content_limit.x : preferred_width, std::max(1.0F, get_size().x - (2 * margin)));
    const float height = std::min(spec.content_limit.y > 0 ? spec.content_limit.y : preferred_height, std::max(1.0F, get_size().y - (2 * margin)));
    panel->set_custom_minimum_size({width, spec.fit_content ? 0.0F : height});
    refresh_menu_chrome(theme);
    if (scroll != nullptr) {
        const float chrome =
            panel->get_theme_stylebox("panel")->get_minimum_size().y + column->get_combined_minimum_size().y - scroll->get_combined_minimum_size().y;
        scroll->set_custom_minimum_size({0, spec.fit_content ? std::min(body->get_combined_minimum_size().y, std::max(0.0F, height - chrome)) : 0.0F});
    }
}

void UiScreenControl::refresh_menu_chrome(const UiThemeData &theme) {
    if (!spec.compact_menu) {
        return;
    }
    {
        const bool compact = context_.phone_landscape && get_size().y < static_cast<float>(theme.metric("menu_compact_height", 280));
        body->add_theme_constant_override(
            "separation", theme.metric(compact ? "menu_compact_button_gap" : "menu_button_separation", compact ? 4 : theme.spacing.section_gap));
        column->add_theme_constant_override("separation", compact ? theme.metric("menu_compact_section_gap", 8) : theme.spacing.section_gap);
        if (title != nullptr) {
            if (compact) {
                title->add_theme_font_size_override("font_size", theme.metric("menu_compact_title_size", 24));
            } else {
                title->remove_theme_font_size_override("font_size");
            }
        }
        if (footer != nullptr) {
            footer->set_visible(!compact || footer->get_child_count() > 0);
        }
    }
}

UiScreenScaffold build_screen(Node *parent, const ScreenSpec &spec, UiScreenControl *controller) {
    UiScreenScaffold scaffold;
    if (parent == nullptr) {
        return scaffold;
    }

    const UiScreenStyle &screen = UiThemeProvider::data().screen;
    if (controller == nullptr) {
        controller = memnew(UiScreenControl);
    }
    controller->spec = spec;
    controller->set_name("ScreenRoot");
    controller->set_process_mode(Node::PROCESS_MODE_ALWAYS);
    controller->set_mouse_filter(Control::MOUSE_FILTER_PASS);
    scaffold.root = controller;
    if (spec.show_backdrop) {
        auto *backdrop = memnew(ColorRect);
        backdrop->set_name("ScreenBackdrop");
        backdrop->set_color(UiThemeProvider::color(screen.backdrop_role));
        backdrop->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        backdrop->set_mouse_filter(Control::MOUSE_FILTER_STOP);
        controller->add_child(backdrop);
    }
    scaffold.root->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    // Keeps the chrome from collapsing when the root is laid out by a container parent instead of anchors.
    // A content-fitted screen has no such floor to impose: its size is whatever its controls need.
    scaffold.root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    scaffold.root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    parent->add_child(scaffold.root);
    // Full-screen views can be mounted below CanvasLayer nodes or launched directly,
    // so they must not depend on a menu scene having installed the shared theme first.
    UiThemeProvider::apply_to(scaffold.root);

    if (spec.manual_content) {
        scaffold.panel = make_surface(screen.panel_surface);
        scaffold.panel->set_name("ScreenPanel");
        scaffold.root->add_child(scaffold.panel);
        scaffold.content = memnew(Control);
        scaffold.content->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        scaffold.panel->add_child(scaffold.content);
        return scaffold;
    }

    auto *center = memnew(CenterContainer);
    center->set_name("ScreenCenter");
    center->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    scaffold.root->add_child(center);

    Control *content_host = center;
    if (spec.panelled_body) {
        auto *panel = make_surface(screen.panel_surface);
        panel->set_name("ScreenPanel");
        center->add_child(panel);
        scaffold.panel = panel;
        controller->panel = panel;
        content_host = panel;
    }

    auto *column = memnew(VBoxContainer);
    column->set_name("ScreenColumn");
    column->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    column->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    column->add_theme_constant_override("separation", UiThemeProvider::spacing("section_gap"));
    content_host->add_child(column);
    controller->column = column;

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
        controller->title = title;
        title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        title->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        header->add_child(title);
    }
    if (!spec.subtitle.is_empty()) {
        auto *subtitle = make_label(spec.subtitle, screen.subtitle_text_style);
        subtitle->set_name("ScreenSubtitle");
        subtitle->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        subtitle->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        header->add_child(subtitle);
    }

    scaffold.body = memnew(VBoxContainer);
    controller->body = scaffold.body;
    scaffold.body->set_name("ScreenBody");
    scaffold.body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    scaffold.body->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    scaffold.body->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));

    if (spec.scrollable_body) {
        auto *scroll = memnew(ScrollContainer);
        scroll->set_name("ScreenScroll");
        controller->scroll = scroll;
        scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
        scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
        scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
        scroll->set_follow_focus(true);
        scroll->add_child(scaffold.body);
        column->add_child(scroll);
    } else {
        column->add_child(scaffold.body);
    }

    scaffold.footer = memnew(HFlowContainer);
    scaffold.footer->set_alignment(FlowContainer::ALIGNMENT_END);
    const int footer_gap = UiThemeProvider::spacing(screen.footer_gap_role);
    scaffold.footer->add_theme_constant_override("h_separation", footer_gap);
    scaffold.footer->add_theme_constant_override("v_separation", footer_gap);
    scaffold.footer->set_name("ScreenFooter");
    column->add_child(scaffold.footer);
    controller->footer = scaffold.footer;
    const auto changed = callable_mp(controller, &UiScreenControl::request_layout);
    for (auto *control : {static_cast<Control *>(column), static_cast<Control *>(scaffold.body), static_cast<Control *>(scaffold.footer)}) {
        control->connect("minimum_size_changed", changed);
        control->connect("visibility_changed", changed);
    }
    controller->set_size(Object::cast_to<Control>(parent) != nullptr ? Object::cast_to<Control>(parent)->get_size() : godot::Vector2(1280, 720));
    controller->refresh_layout();

    return scaffold;
}

} // namespace defn
