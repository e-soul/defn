// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud_meters.h"

#include "godot_color.h"
#include "meter_geometry.h"

#include <algorithm>

namespace defn {

using namespace godot;

namespace {

/// The segment strip's proportions, read once per layout or redraw rather than once per segment.
struct SegmentMetrics {
    real_t width;
    real_t height;
    real_t gap;
    real_t outline_width;

    [[nodiscard]] real_t left_of(int index) const { return static_cast<real_t>(index) * (width + gap); }
    [[nodiscard]] real_t strip_width(int segments) const {
        return segments <= 0 ? 0.0F : (static_cast<real_t>(segments) * width) + (static_cast<real_t>(segments - 1) * gap);
    }
};

} // namespace

HudIntegrityMeter::HudIntegrityMeter() {
    set_name("IntegrityMeter");
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    set_custom_minimum_size({0.0F, segment_height_});
}

void HudIntegrityMeter::_bind_methods() { ClassDB::bind_method(D_METHOD("get_segment_count"), &HudIntegrityMeter::get_segment_count); }

void HudIntegrityMeter::apply_appearance(const UiThemeData &theme) {
    segment_width_ = static_cast<float>(theme.metric("hud_segment_width", 26));
    segment_height_ = static_cast<float>(theme.metric("hud_segment_height", 15));
    segment_gap_ = static_cast<float>(theme.metric("meter_segment_gap", 4));
    outline_width_ = static_cast<float>(theme.metric("meter_outline_width", 1));
    max_width_ = static_cast<float>(theme.metric("hud_max_integrity_width", 160));
    track_color_ = to_godot_color(theme.find_color_role("meter_track").value_or(theme.palette.surface_sunken));
    outline_color_ = to_godot_color(theme.find_color_role("meter_outline").value_or(theme.palette.border));
    const SegmentMetrics metrics{.width = segment_width_, .height = segment_height_, .gap = segment_gap_, .outline_width = outline_width_};
    const godot::Vector2 strip{std::min(metrics.strip_width(model_.segments), max_width_), segment_height_};
    set_custom_minimum_size(strip);
    set_size(strip);
    queue_redraw();
}
void HudIntegrityMeter::configure(const HudIntegrityModel &model, const godot::Color &color) {
    if (model_ == model && color_ == color) {
        return;
    }
    model_ = model;
    color_ = color;
    const SegmentMetrics metrics{.width = segment_width_, .height = segment_height_, .gap = segment_gap_, .outline_width = outline_width_};
    const godot::Vector2 strip{std::min(metrics.strip_width(model_.segments), max_width_), segment_height_};
    if (get_custom_minimum_size() != strip) {
        set_custom_minimum_size(strip);
        set_size(strip);
    }
    queue_redraw();
}

int HudIntegrityMeter::get_segment_count() const { return model_.segments; }

void HudIntegrityMeter::_draw() {
    if (model_.segments <= 0) {
        return;
    }

    SegmentMetrics metrics{.width = segment_width_, .height = segment_height_, .gap = segment_gap_, .outline_width = outline_width_};
    const real_t compression = std::min(1.0F, get_size().x / metrics.strip_width(model_.segments));
    metrics.width *= compression;
    metrics.gap *= compression;
    const real_t height = std::min(metrics.height, get_size().y);

    for (int index = 0; index < model_.segments; ++index) {
        const real_t left = metrics.left_of(index);
        const double fraction = std::clamp(model_.filled_segments - static_cast<double>(index), 0.0, 1.0);
        const PackedVector2Array outline = segment_polygon(left, 0.0F, metrics.width, height);

        draw_colored_polygon(outline, track_color_);
        if (fraction > 0.0) {
            draw_colored_polygon(partial_segment_polygon(left, 0.0F, metrics.width, height, fraction), color_);
        }
        draw_segment_outline(*this, outline, outline_color_, metrics.outline_width);
    }
}

} // namespace defn
