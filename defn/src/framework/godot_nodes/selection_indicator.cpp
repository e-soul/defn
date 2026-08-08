// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "selection_indicator.h"

#include <cmath>
#include <numbers>

#include <godot_cpp/variant/packed_vector2_array.hpp>

namespace defn {

void SelectionIndicator::_bind_methods() {}

void SelectionIndicator::configure(const SelectionIndicatorStyle &style) {
    style_ = style;
    set_z_index(0);
    set_draw_behind_parent(true);
    queue_redraw();
}

void SelectionIndicator::_draw() {
    if (style_.radius_x <= 0.0F || style_.radius_y <= 0.0F) {
        return;
    }

    constexpr int POINT_COUNT = 48;
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

} // namespace defn
