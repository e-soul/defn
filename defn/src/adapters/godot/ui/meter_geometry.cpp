// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "meter_geometry.h"

#include "ui_theme_provider.h"

#include <algorithm>

namespace defn {

namespace {

/// Horizontal offset of a segment's top edge against its bottom edge.
float segment_slope() { return static_cast<float>(UiThemeProvider::data().metric("meter_segment_slope", 5)); }

} // namespace

godot::PackedVector2Array segment_polygon(float left, float top, float width, float height) {
    const float slope = segment_slope();
    godot::PackedVector2Array polygon;
    polygon.push_back({left + slope, top});
    polygon.push_back({left + width, top});
    polygon.push_back({left + width - slope, top + height});
    polygon.push_back({left, top + height});
    return polygon;
}

godot::PackedVector2Array partial_segment_polygon(float left, float top, float width, float height, double fraction) {
    if (fraction >= 1.0) {
        return segment_polygon(left, top, width, height);
    }

    const float right = left + std::max(0.0F, width * static_cast<float>(std::clamp(fraction, 0.0, 1.0)));
    godot::PackedVector2Array polygon;
    polygon.push_back({left + std::min(segment_slope(), right - left), top});
    polygon.push_back({right, top});
    polygon.push_back({right, top + height});
    polygon.push_back({left, top + height});
    return polygon;
}

void draw_segment_outline(godot::CanvasItem &canvas, const godot::PackedVector2Array &polygon, const godot::Color &color, float width) {
    canvas.draw_polyline(polygon, color, width, true);
    canvas.draw_line(polygon[polygon.size() - 1], polygon[0], color, width, true);
}

} // namespace defn
