// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "progression_stat_meter.h"

#include "godot_string.h"
#include "meter_geometry.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace defn {

namespace {

/// Floors that keep the strip drawable when the row is squeezed below its minimum size.
constexpr float MIN_STRIP_WIDTH = 100.0F;
constexpr float MIN_STRIP_HEIGHT = 16.0F;

godot::Color tier_color(int tier, bool neutral) {
    const std::array<const char *, 4> neutral_roles = {"meter_neutral_0", "meter_neutral_1", "meter_neutral_2", "meter_neutral_3"};
    const std::array<const char *, 4> power_roles = {"meter_power_0", "meter_power_1", "meter_power_2", "meter_power_3"};
    const auto &roles = neutral ? neutral_roles : power_roles;
    return UiThemeProvider::color(roles[std::min(static_cast<std::size_t>(std::max(tier, 0)), roles.size() - 1)]);
}

/// The mark each stat draws, named in the theme's shared `icons` map. A stat and the HUD plate that means the
/// same thing therefore draw the same shape, which nine hand-drawn shapes in this file could never guarantee.
std::string_view stat_icon_key(ProgressionStatIcon icon) {
    switch (icon) {
    case ProgressionStatIcon::SHIELD:
        return "plating";
    case ProgressionStatIcon::RETICLE:
        return "target";
    case ProgressionStatIcon::MOBILITY:
        return "speed";
    case ProgressionStatIcon::BATTERY:
        return "battery";
    case ProgressionStatIcon::FIRE_RATE:
        return "cadence";
    case ProgressionStatIcon::INTEGRITY:
        return "bulwark";
    case ProgressionStatIcon::ENERGY:
        return "energy";
    case ProgressionStatIcon::BOUNTY:
        return "salvage";
    case ProgressionStatIcon::GENERIC:
        return "generic";
    }
    return "generic";
}

} // namespace

ProgressionStatMeter::ProgressionStatMeter() {
    set_custom_minimum_size({UiThemeProvider::metric("meter_min_width", 255), UiThemeProvider::metric("meter_height", 36)});
    set_focus_mode(FOCUS_ALL);
    set_mouse_filter(MOUSE_FILTER_STOP);
    connect("mouse_entered", callable_mp(this, &ProgressionStatMeter::show_detail));
    connect("mouse_exited", callable_mp(this, &ProgressionStatMeter::hide_detail));
    connect("focus_entered", callable_mp(this, &ProgressionStatMeter::on_focus_entered));
    connect("focus_exited", callable_mp(this, &ProgressionStatMeter::on_focus_exited));
}

void ProgressionStatMeter::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("get_segment_count"), &ProgressionStatMeter::get_segment_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_stat_id"), &ProgressionStatMeter::get_stat_id);
    ADD_SIGNAL(godot::MethodInfo("detail_state_changed", godot::PropertyInfo(godot::Variant::STRING, "stat_id"),
                                 godot::PropertyInfo(godot::Variant::STRING, "detail"), godot::PropertyInfo(godot::Variant::BOOL, "active")));
}

void ProgressionStatMeter::configure(const ProgressionStatVisualViewModel &model) {
    model_ = model;
    build_icon();
    set_name(to_godot_string("StatMeter_" + model_.stat_id));
    set_tooltip_text(to_godot_string(model_.exact_detail));
    set_accessibility_name(to_godot_string(model_.stat_id));
    set_accessibility_description(to_godot_string(model_.exact_detail));
    queue_redraw();
}

void ProgressionStatMeter::build_icon() {
    if (icon_ != nullptr) {
        remove_child(icon_);
        icon_->queue_free();
    }

    const godot::real_t size = UiThemeProvider::metric("meter_icon_size", 22);
    icon_ = make_icon(stat_icon_key(model_.icon), size);
    icon_->set_modulate(UiThemeProvider::color("meter_icon"));
    const real_t column = UiThemeProvider::metric("meter_icon_column", 30);
    icon_->set_position({(column - size) * 0.5F, (get_custom_minimum_size().y - size) * 0.5F});
    icon_->set_size({size, size});
    add_child(icon_);
}

void ProgressionStatMeter::_draw() {
    const godot::Vector2 size = get_size();
    const float icon_column = UiThemeProvider::metric("meter_icon_column", 30);
    const float gap = UiThemeProvider::metric("meter_segment_gap", 4);
    const float top = UiThemeProvider::metric("meter_inset_y", 7);
    const float height = std::max(MIN_STRIP_HEIGHT, size.y - (top * 2.0F));
    const auto segment_count = static_cast<float>(PROGRESSION_STAT_SEGMENT_COUNT);
    const float available_width = std::max(MIN_STRIP_WIDTH, size.x - icon_column);
    const float segment_width = (available_width - (gap * (segment_count - 1.0F))) / segment_count;
    const bool neutral = model_.direction == ProgressionStatDirection::MORE_IS_EXPENSIVE;

    const godot::Color track_color = UiThemeProvider::color("meter_track");
    const godot::Color outline_color = UiThemeProvider::color("meter_outline");
    const godot::Color notch_color = UiThemeProvider::color("meter_notch");
    const godot::Color upgrade_color = UiThemeProvider::color("meter_upgrade");

    for (std::size_t index = 0; index < model_.segments.size(); ++index) {
        const auto &segment = model_.segments[index];
        const float left = icon_column + (static_cast<float>(index) * (segment_width + gap));
        const float center_x = left + (segment_width * 0.5F);
        const auto outline = segment_polygon(left, top, segment_width, height);
        draw_colored_polygon(outline, track_color);
        if (segment.foundation_tier >= 0) {
            draw_colored_polygon(outline, tier_color(segment.foundation_tier, neutral));
        }
        if (segment.promotion_fraction > 0.0) {
            draw_colored_polygon(partial_segment_polygon(left, top, segment_width, height, segment.promotion_fraction),
                                 tier_color(segment.promoted_tier, neutral));
        }
        draw_segment_outline(*this, outline, outline_color, UiThemeProvider::metric("meter_outline_width", 1));

        const int visible_tier = segment.promotion_fraction > 0.35 ? segment.promoted_tier : segment.foundation_tier;
        const int mark_count = std::clamp(visible_tier, 0, 4);
        for (int mark = 0; mark < mark_count; ++mark) {
            const float mark_offset = (static_cast<float>(mark) * 3.0F) - (static_cast<float>(mark_count - 1) * 1.5F);
            const float mark_x = center_x + mark_offset;
            draw_line({mark_x - 2.0F, top + (height * 0.65F)}, {mark_x + 1.0F, top + (height * 0.35F)}, notch_color, 1.5F, true);
        }
        if (visible_tier >= 3) {
            godot::PackedVector2Array diamond;
            diamond.push_back({center_x, top + 4.0F});
            diamond.push_back({center_x + 3.0F, top + (height * 0.5F)});
            diamond.push_back({center_x, (top + height) - 4.0F});
            diamond.push_back({center_x - 3.0F, top + (height * 0.5F)});
            draw_colored_polygon(diamond, notch_color);
        }
        if (segment.upgrade_emphasis) {
            draw_line({left + 2.0F, top + height - 2.0F}, {left + segment_width - 4.0F, top + height - 2.0F}, upgrade_color, 2.0F, true);
            draw_line({center_x - 3.0F, top + 1.0F}, {center_x, top - 3.0F}, upgrade_color, 2.0F, true);
            draw_line({center_x, top - 3.0F}, {center_x + 3.0F, top + 1.0F}, upgrade_color, 2.0F, true);
        }
    }
    if (has_focus()) {
        draw_rect({{0.0F, 1.0F}, {size.x, size.y - 2.0F}}, UiThemeProvider::color("meter_focus"), false, 2.0F, true);
    }
}

void ProgressionStatMeter::_gui_input(const godot::Ref<godot::InputEvent> &event) {
    if (auto *mouse = godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr());
        mouse != nullptr && mouse->get_button_index() == godot::MOUSE_BUTTON_LEFT) {
        pointer_active_ = mouse->is_pressed();
        pointer_active_ ? show_detail() : hide_detail();
        accept_event();
        return;
    }
    if (auto *touch = godot::Object::cast_to<godot::InputEventScreenTouch>(event.ptr()); touch != nullptr) {
        pointer_active_ = touch->is_pressed();
        pointer_active_ ? show_detail() : hide_detail();
        accept_event();
    }
}

int ProgressionStatMeter::get_segment_count() const { return static_cast<int>(model_.segments.size()); }

godot::String ProgressionStatMeter::get_stat_id() const { return to_godot_string(model_.stat_id); }

void ProgressionStatMeter::show_detail() { emit_signal("detail_state_changed", get_stat_id(), to_godot_string(model_.exact_detail), true); }

void ProgressionStatMeter::hide_detail() {
    if (!has_focus() && !pointer_active_) {
        emit_signal("detail_state_changed", get_stat_id(), to_godot_string(model_.exact_detail), false);
    }
}

void ProgressionStatMeter::on_focus_entered() {
    queue_redraw();
    show_detail();
}

void ProgressionStatMeter::on_focus_exited() {
    queue_redraw();
    hide_detail();
}

} // namespace defn
