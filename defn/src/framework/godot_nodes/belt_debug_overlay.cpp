// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "belt_debug_overlay.h"

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

namespace {

constexpr godot::real_t LINE_WIDTH = 1.0;
const godot::Color LINE_COLOR(1.0F, 0.15F, 0.15F, 1.0F);

} // namespace

BeltDebugOverlay::BeltDebugOverlay() {
    set_visible(false);
    set_z_as_relative(false);
    set_z_index(1000);
}

void BeltDebugOverlay::_bind_methods() {}

void BeltDebugOverlay::configure(godot::real_t viewport_width, godot::real_t upper_y, godot::real_t lower_y) {
    half_span_ = viewport_width;
    upper_y_ = upper_y;
    lower_y_ = lower_y;
    queue_redraw();
}

void BeltDebugOverlay::follow_camera(godot::real_t camera_x) { set_position({camera_x, 0.0F}); }

void BeltDebugOverlay::toggle_visibility() { set_visible(!is_visible()); }

void BeltDebugOverlay::_draw() {
    draw_line({-half_span_, upper_y_}, {half_span_, upper_y_}, LINE_COLOR, LINE_WIDTH, true);
    draw_line({-half_span_, lower_y_}, {half_span_, lower_y_}, LINE_COLOR, LINE_WIDTH, true);
}

} // namespace defn
