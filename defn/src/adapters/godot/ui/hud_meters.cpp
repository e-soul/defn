// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud_meters.h"

#include "meter_geometry.h"
#include "ui_theme_provider.h"

#include <algorithm>

namespace defn {

using namespace godot;

namespace {

/// The segment strip's proportions, read once per layout or redraw rather than once per segment.
struct SegmentMetrics {
    float width;
    float height;
    float gap;
    float outline_width;

    static SegmentMetrics from_theme() {
        const UiThemeData &theme = UiThemeProvider::data();
        return {
            .width = static_cast<float>(theme.metric("hud_segment_width", 26)),
            .height = static_cast<float>(theme.metric("hud_segment_height", 15)),
            .gap = static_cast<float>(UiThemeProvider::spacing("xs")),
            .outline_width = static_cast<float>(theme.metric("hud_segment_outline_width", 1)),
        };
    }

    [[nodiscard]] float left_of(int index) const { return static_cast<float>(index) * (width + gap); }
    [[nodiscard]] float strip_width(int segments) const {
        return segments <= 0 ? 0.0F : (static_cast<float>(segments) * width) + (static_cast<float>(segments - 1) * gap);
    }
};

} // namespace

HudIntegrityMeter::HudIntegrityMeter() {
    set_name("IntegrityMeter");
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    set_custom_minimum_size({0.0F, SegmentMetrics::from_theme().height});
}

void HudIntegrityMeter::_bind_methods() { ClassDB::bind_method(D_METHOD("get_segment_count"), &HudIntegrityMeter::get_segment_count); }

void HudIntegrityMeter::configure(const HudIntegrityModel &model, const godot::Color &color) {
    if (model == model_ && color == color_) {
        return;
    }

    // The strip only has to be re-measured when it gains or loses a segment; draining the leading one does not
    // move its edges, and integrity drains far more often than a match changes its capacity.
    const bool resized = model.segments != model_.segments;
    model_ = model;
    color_ = color;

    if (resized) {
        const SegmentMetrics metrics = SegmentMetrics::from_theme();
        const godot::Vector2 strip{metrics.strip_width(model_.segments), metrics.height};
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

    const SegmentMetrics metrics = SegmentMetrics::from_theme();
    const float height = std::min(metrics.height, get_size().y);
    const godot::Color track_color = UiThemeProvider::color("meter_track");
    const godot::Color outline_color = UiThemeProvider::color("meter_outline");

    for (int index = 0; index < model_.segments; ++index) {
        const float left = metrics.left_of(index);
        const double fraction = std::clamp(model_.filled_segments - static_cast<double>(index), 0.0, 1.0);
        const PackedVector2Array outline = segment_polygon(left, 0.0F, metrics.width, height);

        draw_colored_polygon(outline, track_color);
        if (fraction > 0.0) {
            draw_colored_polygon(partial_segment_polygon(left, 0.0F, metrics.width, height, fraction), color_);
        }
        draw_segment_outline(*this, outline, outline_color, metrics.outline_width);
    }
}

} // namespace defn
