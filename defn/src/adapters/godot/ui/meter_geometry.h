// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef METER_GEOMETRY_H
#define METER_GEOMETRY_H

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

namespace defn {

/// The skewed segment shared by the progression stat meters and the HUD integrity meter. The lean is what makes
/// the two read as the same instrument family, so it comes from one metric rather than a constant per meter.
[[nodiscard]] godot::PackedVector2Array segment_polygon(float left, float top, float width, float height);

/// The same segment clipped to `fraction` of its width, for a bar that drains continuously rather than in steps.
[[nodiscard]] godot::PackedVector2Array partial_segment_polygon(float left, float top, float width, float height, double fraction);

/// Strokes a segment. `draw_polyline` leaves a polygon open, so the closing edge is drawn separately.
void draw_segment_outline(godot::CanvasItem &canvas, const godot::PackedVector2Array &polygon, const godot::Color &color, float width);

} // namespace defn

#endif
