// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "reposition_destination_marker.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <godot_cpp/variant/packed_vector2_array.hpp>

namespace defn {

void RepositionDestinationMarker::_bind_methods() {}

void RepositionDestinationMarker::configure(const RepositionDestinationMarkerStyle &style) {
    style_ = style;
    elapsed_seconds_ = 0.0;
    set_scale({style_.minimum_scale, style_.minimum_scale});
    queue_redraw();
}

void RepositionDestinationMarker::_draw() {
    if (style_.radius_x <= 0.0F || style_.radius_y <= 0.0F) {
        return;
    }

    constexpr int POINT_COUNT = 36;
    PackedVector2Array points;
    points.resize(POINT_COUNT);
    for (int index = 0; index < POINT_COUNT; ++index) {
        const real_t angle = 2.0F * std::numbers::pi_v<real_t> * static_cast<real_t>(index) / static_cast<real_t>(POINT_COUNT);
        points.set(index, {std::cos(angle) * style_.radius_x, std::sin(angle) * style_.radius_y});
    }
    draw_colored_polygon(points, style_.fill_color);
    points.push_back(points[0]);
    draw_polyline(points, style_.border_color, style_.border_width, true);
}

void RepositionDestinationMarker::_process(double delta) {
    const double pulse_duration = std::max(style_.pulse_duration_seconds, 0.01);
    const int pulse_count = std::max(style_.pulse_count, 1);
    const double total_duration = pulse_duration * static_cast<double>(pulse_count);
    elapsed_seconds_ += std::max(delta, 0.0);
    if (elapsed_seconds_ >= total_duration) {
        queue_free();
        return;
    }

    const double phase = std::fmod(elapsed_seconds_, pulse_duration) / pulse_duration;
    const auto pulse = static_cast<real_t>(std::sin(std::numbers::pi * phase));
    const real_t scale = std::lerp(style_.minimum_scale, style_.maximum_scale, pulse);
    set_scale({scale, scale});

    const auto remaining = static_cast<real_t>(1.0 - (elapsed_seconds_ / total_duration));
    const real_t alpha = std::clamp((0.35F + (0.65F * pulse)) * std::min(remaining * 4.0F, 1.0F), 0.0F, 1.0F);
    set_modulate({1.0F, 1.0F, 1.0F, alpha});
}

} // namespace defn
