#include "progression_stat_meter.h"

#include "godot_string.h"

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

namespace defn {

namespace {

constexpr float ICON_WIDTH = 30.0F;
constexpr float SEGMENT_GAP = 5.0F;
constexpr float SEGMENT_SLOPE = 5.0F;

godot::Color tier_color(int tier, bool neutral) {
    if (neutral) {
        const std::array<godot::Color, 4> neutral_colors = {godot::Color(0.30, 0.40, 0.50), godot::Color(0.48, 0.57, 0.64), godot::Color(0.67, 0.73, 0.77),
                                                            godot::Color(0.84, 0.87, 0.89)};
        return neutral_colors[std::min(static_cast<std::size_t>(std::max(tier, 0)), neutral_colors.size() - 1)];
    }
    const std::array<godot::Color, 4> power_colors = {godot::Color(0.29, 0.52, 0.72), godot::Color(0.95, 0.70, 0.20), godot::Color(0.62, 0.40, 0.82),
                                                      godot::Color(0.86, 0.91, 0.96)};
    return power_colors[std::min(static_cast<std::size_t>(std::max(tier, 0)), power_colors.size() - 1)];
}

godot::PackedVector2Array segment_polygon(float left, float top, float width, float height) {
    godot::PackedVector2Array polygon;
    polygon.push_back({left + SEGMENT_SLOPE, top});
    polygon.push_back({left + width, top});
    polygon.push_back({left + width - SEGMENT_SLOPE, top + height});
    polygon.push_back({left, top + height});
    return polygon;
}

godot::PackedVector2Array partial_segment_polygon(float left, float top, float width, float height, double fraction) {
    const float right = left + std::max(0.0F, width * static_cast<float>(std::clamp(fraction, 0.0, 1.0)));
    if (fraction >= 1.0) {
        return segment_polygon(left, top, width, height);
    }
    godot::PackedVector2Array polygon;
    polygon.push_back({left + std::min(SEGMENT_SLOPE, right - left), top});
    polygon.push_back({right, top});
    polygon.push_back({right, top + height});
    polygon.push_back({left, top + height});
    return polygon;
}

void draw_icon(ProgressionStatMeter &meter, ProgressionStatIcon icon, const godot::Color &color, float center_y) {
    const godot::Vector2 center(15.0F, center_y);
    switch (icon) {
    case ProgressionStatIcon::SHIELD: {
        godot::PackedVector2Array shield;
        shield.push_back({8.0F, center_y - 9.0F});
        shield.push_back({22.0F, center_y - 9.0F});
        shield.push_back({21.0F, center_y + 3.0F});
        shield.push_back({15.0F, center_y + 10.0F});
        shield.push_back({9.0F, center_y + 3.0F});
        meter.draw_polyline(shield, color, 2.0F, true);
        meter.draw_line(shield[4], shield[0], color, 2.0F, true);
        break;
    }
    case ProgressionStatIcon::RETICLE:
        meter.draw_arc(center, 8.0F, 0.0F, 6.2831853F, 20, color, 2.0F, true);
        meter.draw_line({4.0F, center_y}, {26.0F, center_y}, color, 2.0F, true);
        meter.draw_line({15.0F, center_y - 11.0F}, {15.0F, center_y + 11.0F}, color, 2.0F, true);
        break;
    case ProgressionStatIcon::MOBILITY:
        meter.draw_line({5.0F, center_y - 7.0F}, {13.0F, center_y}, color, 2.0F, true);
        meter.draw_line({13.0F, center_y}, {5.0F, center_y + 7.0F}, color, 2.0F, true);
        meter.draw_line({14.0F, center_y - 7.0F}, {22.0F, center_y}, color, 2.0F, true);
        meter.draw_line({22.0F, center_y}, {14.0F, center_y + 7.0F}, color, 2.0F, true);
        break;
    case ProgressionStatIcon::BATTERY:
    case ProgressionStatIcon::ENERGY:
        meter.draw_rect({{6.0F, center_y - 8.0F}, {17.0F, 16.0F}}, color, false, 2.0F, true);
        meter.draw_rect({{23.0F, center_y - 3.0F}, {3.0F, 6.0F}}, color, true);
        if (icon == ProgressionStatIcon::ENERGY) {
            meter.draw_line({16.0F, center_y - 6.0F}, {11.0F, center_y + 1.0F}, color, 2.0F, true);
            meter.draw_line({11.0F, center_y + 1.0F}, {17.0F, center_y}, color, 2.0F, true);
            meter.draw_line({17.0F, center_y}, {13.0F, center_y + 6.0F}, color, 2.0F, true);
        }
        break;
    case ProgressionStatIcon::FIRE_RATE:
        for (float offset : {7.0F, 14.0F, 21.0F}) {
            meter.draw_line({offset, center_y - 7.0F}, {offset + 4.0F, center_y + 7.0F}, color, 2.0F, true);
        }
        break;
    case ProgressionStatIcon::INTEGRITY:
        meter.draw_circle(center, 8.0F, color, false, 2.0F, true);
        meter.draw_line({15.0F, center_y - 5.0F}, {15.0F, center_y + 5.0F}, color, 2.0F, true);
        meter.draw_line({10.0F, center_y}, {20.0F, center_y}, color, 2.0F, true);
        break;
    case ProgressionStatIcon::BOUNTY:
        meter.draw_circle(center, 9.0F, color, false, 2.0F, true);
        meter.draw_circle(center, 4.0F, color, false, 1.5F, true);
        break;
    case ProgressionStatIcon::GENERIC:
        meter.draw_rect({{7.0F, center_y - 8.0F}, {16.0F, 16.0F}}, color, false, 2.0F, true);
        meter.draw_line({11.0F, center_y}, {19.0F, center_y}, color, 2.0F, true);
        break;
    }
}

} // namespace

ProgressionStatMeter::ProgressionStatMeter() {
    set_custom_minimum_size({255.0F, 36.0F});
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
    set_name(to_godot_string("StatMeter_" + model_.stat_id));
    set_tooltip_text(to_godot_string(model_.exact_detail));
    set_accessibility_name(to_godot_string(model_.stat_id));
    set_accessibility_description(to_godot_string(model_.exact_detail));
    queue_redraw();
}

void ProgressionStatMeter::_draw() {
    const godot::Vector2 size = get_size();
    const float top = 7.0F;
    const float height = std::max(16.0F, size.y - 14.0F);
    const float available_width = std::max(100.0F, size.x - ICON_WIDTH);
    const float segment_width = (available_width - SEGMENT_GAP * 4.0F) / 5.0F;
    const bool neutral = model_.direction == ProgressionStatDirection::MORE_IS_EXPENSIVE;
    draw_icon(*this, model_.icon, godot::Color(0.76, 0.83, 0.90), size.y / 2.0F);

    for (std::size_t index = 0; index < model_.segments.size(); ++index) {
        const auto &segment = model_.segments[index];
        const float left = ICON_WIDTH + (static_cast<float>(index) * (segment_width + SEGMENT_GAP));
        const float center_x = left + (segment_width * 0.5F);
        const auto outline = segment_polygon(left, top, segment_width, height);
        draw_colored_polygon(outline, godot::Color(0.08, 0.13, 0.19));
        if (segment.foundation_tier >= 0) {
            draw_colored_polygon(outline, tier_color(segment.foundation_tier, neutral));
        }
        if (segment.promotion_fraction > 0.0) {
            draw_colored_polygon(partial_segment_polygon(left, top, segment_width, height, segment.promotion_fraction),
                                 tier_color(segment.promoted_tier, neutral));
        }
        draw_polyline(outline, godot::Color(0.43, 0.55, 0.66), 1.5F, true);
        draw_line(outline[3], outline[0], godot::Color(0.43, 0.55, 0.66), 1.5F, true);

        const int visible_tier = segment.promotion_fraction > 0.35 ? segment.promoted_tier : segment.foundation_tier;
        const int mark_count = std::clamp(visible_tier, 0, 4);
        for (int mark = 0; mark < mark_count; ++mark) {
            const float mark_offset = (static_cast<float>(mark) * 3.0F) - (static_cast<float>(mark_count - 1) * 1.5F);
            const float mark_x = center_x + mark_offset;
            draw_line({mark_x - 2.0F, top + (height * 0.65F)}, {mark_x + 1.0F, top + (height * 0.35F)}, godot::Color(0.05, 0.08, 0.12), 1.5F, true);
        }
        if (visible_tier >= 3) {
            godot::PackedVector2Array diamond;
            diamond.push_back({center_x, top + 4.0F});
            diamond.push_back({center_x + 3.0F, top + (height * 0.5F)});
            diamond.push_back({center_x, (top + height) - 4.0F});
            diamond.push_back({center_x - 3.0F, top + (height * 0.5F)});
            draw_colored_polygon(diamond, godot::Color(0.05, 0.08, 0.12));
        }
        if (segment.upgrade_emphasis) {
            const godot::Color upgrade_color(0.45, 0.90, 0.95);
            draw_line({left + 2.0F, top + height - 2.0F}, {left + segment_width - 4.0F, top + height - 2.0F}, upgrade_color, 2.0F, true);
            draw_line({center_x - 3.0F, top + 1.0F}, {center_x, top - 3.0F}, upgrade_color, 2.0F, true);
            draw_line({center_x, top - 3.0F}, {center_x + 3.0F, top + 1.0F}, upgrade_color, 2.0F, true);
        }
    }
    if (has_focus()) {
        draw_rect({{0.0F, 1.0F}, {size.x, size.y - 2.0F}}, godot::Color(0.55, 0.78, 0.95), false, 2.0F, true);
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
