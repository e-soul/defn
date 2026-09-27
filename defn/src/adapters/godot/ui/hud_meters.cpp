// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud_meters.h"

#include "meter_geometry.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/font.hpp>

#include <algorithm>
#include <cmath>

namespace defn {

using namespace godot;

namespace {

/// The segment strip's proportions, read once per layout or redraw rather than once per segment.
struct SegmentMetrics {
    real_t width;
    real_t height;
    real_t gap;
    real_t outline_width;

    static SegmentMetrics from_theme() {
        return {
            .width = UiThemeProvider::metric("hud_segment_width", 26),
            .height = UiThemeProvider::metric("hud_segment_height", 15),
            .gap = UiThemeProvider::metric("meter_segment_gap", 4),
            .outline_width = UiThemeProvider::metric("meter_outline_width", 1),
        };
    }

    [[nodiscard]] real_t left_of(int index) const { return static_cast<real_t>(index) * (width + gap); }
    [[nodiscard]] real_t strip_width(int segments) const {
        return segments <= 0 ? 0.0F : (static_cast<real_t>(segments) * width) + (static_cast<real_t>(segments - 1) * gap);
    }
};

} // namespace

HudIntegrityMeter::HudIntegrityMeter() {
    set_name("IntegrityMeter");
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    set_custom_minimum_size({0.0F, SegmentMetrics::from_theme().height});
    percentage_label_ = make_label("", "hud_wave_total");
    percentage_label_->set_name("IntegrityPercentage");
    percentage_label_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    percentage_label_->set_clip_text(true);
    percentage_label_->hide();
    add_child(percentage_label_);
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
    const double fraction = model_.segments > 0 ? model_.filled_segments / model_.segments : 0.0;
    percentage_label_->set_text(String::num_int64(static_cast<int64_t>(std::round(std::clamp(fraction, 0.0, 1.0) * 100.0))) + "%");

    if (resized) {
        update_layout();
    }
    queue_redraw();
}

void HudIntegrityMeter::set_layout(float scale, float maximum_width) {
    if (layout_scale_ == scale && maximum_width_ == maximum_width) {
        return;
    }
    layout_scale_ = scale;
    maximum_width_ = maximum_width;
    update_layout();
    queue_redraw();
}

void HudIntegrityMeter::update_layout() {
    const SegmentMetrics metrics = SegmentMetrics::from_theme();
    const float natural_width = metrics.strip_width(model_.segments) * layout_scale_;
    const float width = maximum_width_ > 0.0F ? std::min(natural_width, maximum_width_) : natural_width;
    const bool continuous = model_.segments > 0 && width / static_cast<float>(model_.segments) < 6.0F * layout_scale_;
    percentage_label_->set_visible(continuous);
    const float bar_height = metrics.height * layout_scale_;
    // Measure the shared font directly: Label minimum-size caches can still contain the previous viewport's
    // typography during rotation. Reserve 100% so damage does not shift the bar and percentage around.
    const Ref<Font> font = percentage_label_->get_theme_font("font");
    const int font_size = UiThemeProvider::font_size("body");
    const godot::Vector2 label_size{font->get_string_size("100%", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x, font->get_height(font_size)};
    const float height = continuous ? std::max(bar_height, label_size.y) : bar_height;
    const float gap = static_cast<float>(UiThemeProvider::spacing("xs")) * layout_scale_;
    continuous_bar_width_ = std::max(0.0F, width - label_size.x - gap);
    continuous_bar_y_ = (height - bar_height) * 0.5F;
    percentage_label_->set_position({width - label_size.x, (height - label_size.y) * 0.5F});
    percentage_label_->set_size(label_size);
    const godot::Vector2 strip{width, height};
    set_custom_minimum_size(strip);
    set_size(strip);
}

int HudIntegrityMeter::get_segment_count() const { return model_.segments; }

void HudIntegrityMeter::_draw() {
    if (model_.segments <= 0) {
        return;
    }

    SegmentMetrics metrics = SegmentMetrics::from_theme();
    metrics.width *= layout_scale_;
    metrics.height *= layout_scale_;
    metrics.gap *= layout_scale_;
    metrics.outline_width *= layout_scale_;
    const float horizontal_scale = std::min(1.0F, get_size().x / metrics.strip_width(model_.segments));
    metrics.width *= horizontal_scale;
    metrics.gap *= horizontal_scale;
    const real_t height = std::min(metrics.height, get_size().y);
    const godot::Color track_color = UiThemeProvider::color("meter_track");
    const godot::Color outline_color = UiThemeProvider::color("meter_outline");

    if (percentage_label_->is_visible()) {
        // At extreme capacities, individual segments cease to be useful. Keep the same bounded strip as a
        // continuous meter instead of drawing hundreds of illegible slivers or widening the top plate.
        const godot::Rect2 track{{0.0F, continuous_bar_y_}, {continuous_bar_width_, height}};
        draw_rect(track, track_color);
        const float fraction = static_cast<float>(std::clamp(model_.filled_segments / model_.segments, 0.0, 1.0));
        draw_rect({track.position, {continuous_bar_width_ * fraction, height}}, color_);
        draw_rect(track, outline_color, false, metrics.outline_width);
        return;
    }

    for (int index = 0; index < model_.segments; ++index) {
        const real_t left = metrics.left_of(index);
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
