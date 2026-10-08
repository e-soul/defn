// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef RESPONSIVE_LAYOUT_H
#define RESPONSIVE_LAYOUT_H

#include "ui_theme_models.h"
#include <cstddef>

namespace defn {
struct UiSize {
    float width = 1280;
    float height = 720;
    bool operator==(const UiSize &) const = default;
};
struct UiInsets {
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
    bool operator==(const UiInsets &) const = default;
};
struct DisplaySnapshot {
    UiSize content;
    UiInsets safe_area;
    bool safe_area_applied = false;
    float render_density = 1;
    bool touch = false;
    bool fine_pointer = true;
    // A phone can expose a secondary mouse while its primary pointer remains touch.
    bool primary_coarse_pointer = false;
    bool operator==(const DisplaySnapshot &) const = default;
};
struct UiRect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
};
// Children receive usable, root-local geometry. Pointer targets and composition remain separate choices.
struct UiContext {
    DisplaySnapshot display;
    UiProfile profile = UiProfile::Standard;
    bool desktop_match = false;
    bool phone_landscape = false;
    bool mobile_score = false;
    bool wide_campaign = false;
};
[[nodiscard]] UiContext resolve_ui_context(const DisplaySnapshot &display, const UiResponsiveData &data, UiProfile profile);
[[nodiscard]] bool uses_mobile_menu_layout(const DisplaySnapshot &display);
enum class LayoutFamily { Tall, Landscape };
enum class TrayPlacement { External, Sky, Bottom, Gutter };
enum class HudArrangement { Instruments, Flow, Gutters, SingleRow };
struct TrayLayout {
    UiRect clip;
    UiRect previous;
    UiRect next;
    UiSize content{0, 0};
    int columns = 1;
    float max_scroll = 0;
    float centered_offset = 0;
    bool overflow = false;
};
[[nodiscard]] TrayLayout resolve_tray_layout(UiSize available, UiCardGeometry card, float gap, float navigation, std::size_t count, bool tall,
                                             bool navigation_below, bool center);
struct HudLayoutMetrics {
    UiSize single_row{0, 0};
    float flow_height = 0;
    float instruments_width = 0;
    float instruments_height = 0;
    UiSize primary_gutter{0, 0};
    UiSize secondary_gutter{0, 0};
};
struct MatchLayout {
    UiRect usable;
    UiRect battlefield;
    UiRect metrics;
    UiRect details;
    UiRect tray;
    UiRect pause;
    float metrics_scale = 1;
    LayoutFamily family = LayoutFamily::Landscape;
    TrayPlacement placement = TrayPlacement::External;
    HudArrangement hud = HudArrangement::Flow;
    bool tray_navigation_below = false;
    bool center_cards = false;
    int columns = 1;
    // Desktop instruments and tray use reference coordinates; battlefield remains in logical screen units.
    bool desktop_reference = false;
    UiSize reference;
};

[[nodiscard]] DisplaySnapshot normalize_display_snapshot(DisplaySnapshot display);
[[nodiscard]] UiRect usable_rect(const DisplaySnapshot &display);
[[nodiscard]] bool uses_mobile_score_layout(const DisplaySnapshot &display);
[[nodiscard]] bool is_phone_landscape(const DisplaySnapshot &display, const UiResponsiveData &data);
[[nodiscard]] UiProfile resolve_ui_profile(const DisplaySnapshot &display, const UiResponsiveData &data, UiProfile previous);
[[nodiscard]] UiThemeData resolve_ui_theme(const UiThemeData &source, UiProfile profile, bool touch);
[[nodiscard]] MatchLayout resolve_desktop_match_layout(const DisplaySnapshot &display, const UiResponsiveData &data, UiSize world, std::size_t roster);
[[nodiscard]] MatchLayout resolve_match_layout(const DisplaySnapshot &display, const UiResponsiveData &data, UiCardGeometry card, UiSize world,
                                               std::size_t roster, HudLayoutMetrics measured = {}, float sky_left = 0);
} // namespace defn
#endif
