// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "menu_backdrop.h"

#include "ui_theme_provider.h"

#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include <cmath>

namespace defn {

using namespace godot;

namespace {

/// The composition, in fractions of the viewport, so it reads the same at every resolution.
constexpr float VIGNETTE_HEIGHT = 0.26F;

/// The hatch runs with the belt's perspective: one stripe per this fraction of the width, leaning by this much
/// of the height. Both are pure look, tuned so the texture reads without competing with the menu column.
constexpr float HATCH_SPACING = 1.0F / 22.0F;
constexpr float HATCH_TILT = 0.30F;
constexpr float HATCH_WIDTH = 2.0F;

constexpr float HATCH_ALPHA = 0.085F;
constexpr float CONTEST_ALPHA = 0.13F;
constexpr float VIGNETTE_ALPHA = 0.50F;

godot::Color at_alpha(godot::Color color, float alpha) {
    color.a = alpha;
    return color;
}

godot::PackedVector2Array quad(const godot::Rect2 &rect) {
    godot::PackedVector2Array points;
    points.push_back(rect.position);
    points.push_back({rect.position.x + rect.size.x, rect.position.y});
    points.push_back(rect.position + rect.size);
    points.push_back({rect.position.x, rect.position.y + rect.size.y});
    return points;
}

/// A quad shaded between two colours. `horizontal` runs the ramp left to right; otherwise it runs top to bottom.
godot::PackedColorArray ramp(const godot::Color &start, const godot::Color &finish, bool horizontal) {
    godot::PackedColorArray colors;
    colors.push_back(start);
    colors.push_back(horizontal ? finish : start);
    colors.push_back(finish);
    colors.push_back(horizontal ? start : finish);
    return colors;
}

} // namespace

MenuBackdrop::MenuBackdrop() {
    set_name("Backdrop");
    set_anchors_preset(PRESET_FULL_RECT);
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    // The hatch is drawn past the edges so every stripe stays parallel; the clip is what squares it off.
    set_clip_contents(true);
}

void MenuBackdrop::_bind_methods() {}

void MenuBackdrop::_notification(int what) {
    if (what == NOTIFICATION_RESIZED) {
        queue_redraw();
    }
}

void MenuBackdrop::_draw() {
    const godot::Vector2 size = get_size();
    if (size.x <= 0.0F || size.y <= 0.0F) {
        return;
    }

    const godot::Color sunken = UiThemeProvider::color("surface_sunken");
    const godot::Color surface = UiThemeProvider::color("surface");
    const godot::Color border = UiThemeProvider::color("border");
    const godot::Color ink = UiThemeProvider::color("overlay_scrim");

    // Sky to ground: the darkest band is overhead, so the plate settles instead of glaring at the top.
    draw_polygon(quad({{0.0F, 0.0F}, size}), ramp(sunken, surface, false));

    const float hatch_step = size.x * HATCH_SPACING;
    const float hatch_lean = size.y * HATCH_TILT;
    const auto hatch_count = static_cast<int>(std::ceil((size.x + hatch_lean) / hatch_step));
    for (int index = 0; index < hatch_count; ++index) {
        const float left = (static_cast<float>(index) * hatch_step) - hatch_lean;
        draw_line({left, 0.0F}, {left + hatch_lean, size.y}, at_alpha(border, HATCH_ALPHA), HATCH_WIDTH, true);
    }

    // The tug-of-war itself: each side's colour bleeds in from its own edge and gives out at the centre line.
    const float half = size.x * 0.5F;
    const godot::Color friendly = UiThemeProvider::color("energy");
    const godot::Color hostile = UiThemeProvider::color("state_danger");
    draw_polygon(quad({{0.0F, 0.0F}, {half, size.y}}), ramp(at_alpha(friendly, CONTEST_ALPHA), at_alpha(friendly, 0.0F), true));
    draw_polygon(quad({{half, 0.0F}, {half, size.y}}), ramp(at_alpha(hostile, 0.0F), at_alpha(hostile, CONTEST_ALPHA), true));

    const float vignette = size.y * VIGNETTE_HEIGHT;
    draw_polygon(quad({{0.0F, 0.0F}, {size.x, vignette}}), ramp(at_alpha(ink, VIGNETTE_ALPHA), at_alpha(ink, 0.0F), false));
    draw_polygon(quad({{0.0F, size.y - vignette}, {size.x, vignette}}), ramp(at_alpha(ink, 0.0F), at_alpha(ink, VIGNETTE_ALPHA), false));
}

} // namespace defn
