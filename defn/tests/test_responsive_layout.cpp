// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "responsive_layout.h"
#include "test_harness.h"
#include <limits>
#include <set>

namespace {
using namespace defn;
DEFN_TEST(score_layout_keeps_primary_touch_mobile_when_a_secondary_mouse_is_available) {
    // Android Chrome in the emulator reports all three capabilities at once.
    DisplaySnapshot phone{{411, 785}, {}, true, 2.625F, true, true, true};
    DEFN_CHECK(uses_mobile_score_layout(phone));
    phone.content = {914, 285};
    DEFN_CHECK(uses_mobile_score_layout(phone));
    phone.fine_pointer = false;
    phone.primary_coarse_pointer = false;
    DEFN_CHECK(uses_mobile_score_layout(phone));

    const DisplaySnapshot touch_desktop{{1920, 1080}, {}, true, 1, true, true, false};
    DEFN_CHECK(!uses_mobile_score_layout(touch_desktop));
    DEFN_CHECK(!uses_mobile_score_layout({{390, 783}}));
}
DEFN_TEST(phone_landscape_centers_one_row_at_top_and_docks_thin_cards_at_bottom) {
    const UiResponsiveData data;
    for (const UiSize size : {UiSize{667, 322}, UiSize{864, 362}, UiSize{800, 230}}) {
        DisplaySnapshot display{size, {24, 0, 8, 12}, false, 3, true, false};
        const auto area = usable_rect(display);
        const auto layout = resolve_match_layout(display, data, data.small_card, {1920, 1080}, 4, {.single_row = {480, 30}});
        DEFN_CHECK(is_phone_landscape(display, data));
        DEFN_CHECK(layout.hud == HudArrangement::SingleRow);
        DEFN_CHECK_EQ(layout.metrics.y, area.y + data.phone_edge_margin);
        DEFN_CHECK_CLOSE(layout.metrics.x + layout.metrics.width / 2, area.x + area.width / 2, 0.01F);
        DEFN_CHECK_EQ(layout.tray.y + layout.tray.height, area.y + area.height - data.phone_edge_margin);
        DEFN_CHECK_EQ(layout.tray.height, 48);
        DEFN_CHECK_CLOSE(layout.battlefield.height, area.height, 0.01F);
        DEFN_CHECK(layout.metrics.x + layout.metrics.width + data.gap <= layout.pause.x);
        DEFN_CHECK_EQ(layout.pause.width, data.touch_target);
        DEFN_CHECK(layout.center_cards);
    }
    DisplaySnapshot narrow{{640, 300}, {}, true, 2, true, false};
    const auto large = resolve_match_layout(narrow, data, data.small_card, {1920, 1080}, 10, {.single_row = {750, 30}});
    DEFN_CHECK(large.metrics_scale < 1);
    DEFN_CHECK_CLOSE(large.metrics.x + large.metrics.width / 2, 320, 0.01F);
    DEFN_CHECK(!is_phone_landscape({{390, 844}, {}, true, 3, true, false}, data));
    DEFN_CHECK(!is_phone_landscape({{1024, 768}, {}, true, 2, true, false}, data));
    DEFN_CHECK(!is_phone_landscape({{960, 540}}, data));
}
DEFN_TEST(desktop_reference_fills_the_field_and_scales_the_original_overlay_with_it) {
    UiResponsiveData data;
    data.margin = 24;
    data.gap = 12;
    data.standard_card = {190, 110, 80};
    for (const float density : {1.0F, 1.5F, 2.0F, 3.0F}) {
        DisplaySnapshot display{{1920 / density, 1080 / density}};
        display.render_density = density;
        const auto layout = resolve_desktop_match_layout(display, data, {1920, 1080}, 4);
        DEFN_CHECK(layout.desktop_reference);
        DEFN_CHECK_CLOSE(layout.battlefield.height * density, 1080, 0.01F);
        DEFN_CHECK_CLOSE(layout.battlefield.width * density, 1920, 0.01F);
        DEFN_CHECK_CLOSE(layout.battlefield.x, 0, 0.01F);
        DEFN_CHECK_CLOSE(layout.battlefield.y, 0, 0.01F);
        DEFN_CHECK_EQ(layout.tray.y, 946);
        DEFN_CHECK_EQ(layout.tray.height, 110);
        DEFN_CHECK(layout.center_cards);
    }
    const auto ultrawide = resolve_desktop_match_layout({{2560, 1080}}, data, {1920, 1080}, 4);
    DEFN_CHECK_EQ(ultrawide.battlefield.x, 320);
    DEFN_CHECK_EQ(ultrawide.battlefield.height, 1080);
}
DEFN_TEST(responsive_density_and_safe_area_are_independent_of_geometry) {
    DisplaySnapshot display{{390, 844}, {0, 24, 0, 16}, false, 1, true, false};
    const UiResponsiveData data;
    const auto low = resolve_match_layout(display, data, data.small_card, {1920, 1080}, 10);
    display.render_density = 3;
    const auto high = resolve_match_layout(display, data, data.small_card, {1920, 1080}, 10);
    DEFN_CHECK_CLOSE(low.battlefield.width, high.battlefield.width, 0.01F);
    DEFN_CHECK_CLOSE(low.battlefield.height, low.battlefield.width * 9 / 16, 0.01F);
    DEFN_CHECK(low.metrics.y >= 24);
    DEFN_CHECK(low.metrics.y + low.metrics.height <= low.battlefield.y);
    DEFN_CHECK(low.tray.y >= low.battlefield.y + low.battlefield.height);
    DEFN_CHECK(low.tray.y + low.tray.height <= 828);
    display.safe_area_applied = true;
    DEFN_CHECK_EQ(usable_rect(display).height, 844);
}
DEFN_TEST(responsive_profiles_use_four_slots_and_hysteresis) {
    UiThemeData source;
    const auto standard = resolve_ui_theme(source, UiProfile::Standard, false);
    const auto small = resolve_ui_theme(source, UiProfile::Small, true);
    DEFN_CHECK(small.typography.body > standard.typography.body);
    DEFN_CHECK_EQ(small.typography.body, 18);
    std::set<int> sizes;
    for (const char *role : {"banner", "display", "title", "menu", "section", "heading", "stat", "subheading", "body", "caption", "card_body", "micro"}) {
        sizes.insert(*small.find_font_size_role(role));
    }
    DEFN_CHECK_EQ(sizes.size(), 4U);
    DisplaySnapshot display{{800, 599}};
    DEFN_CHECK(resolve_ui_profile(display, source.responsive, UiProfile::Standard) == UiProfile::Standard);
    DEFN_CHECK(resolve_ui_profile(display, source.responsive, UiProfile::Small) == UiProfile::Small);
    display.content.height = 560;
    DEFN_CHECK(resolve_ui_profile(display, source.responsive, UiProfile::Standard) == UiProfile::Small);
    display.content.height = 620;
    DEFN_CHECK(resolve_ui_profile(display, source.responsive, UiProfile::Small) == UiProfile::Standard);
}
DEFN_TEST(responsive_landscape_strip_preserves_protected_world_and_reachable_overflow) {
    const UiResponsiveData data;
    for (const UiSize content : {UiSize{667, 330}, UiSize{844, 390}, UiSize{1280, 720}, UiSize{2560, 1080}}) {
        const auto layout = resolve_match_layout({content}, data, data.small_card, {1920, 1080}, 10);
        DEFN_CHECK(layout.family == LayoutFamily::Landscape);
        DEFN_CHECK_EQ(layout.columns, 1);
        DEFN_CHECK(layout.battlefield.width > 0);
        DEFN_CHECK_CLOSE(layout.battlefield.width / layout.battlefield.height, 16.0F / 9.0F, 0.001F);
        if (layout.placement == TrayPlacement::Sky) {
            DEFN_CHECK(layout.tray.y + layout.tray.height <= layout.battlefield.y + layout.battlefield.height * data.sky_bottom);
        } else if (layout.placement == TrayPlacement::External) {
            DEFN_CHECK(layout.tray.y >= layout.battlefield.y + layout.battlefield.height);
        }
    }
}
DEFN_TEST(responsive_short_wide_stage_uses_side_gutter_for_metrics_and_horizontal_dock) {
    const UiResponsiveData data;
    const auto layout = resolve_match_layout({{864, 230}}, data, data.small_card, {1920, 1080}, 10,
                                             {.flow_height = 80, .primary_gutter = {200, 192}, .secondary_gutter = {192, 72}});
    DEFN_CHECK(layout.family == LayoutFamily::Landscape);
    DEFN_CHECK_EQ(layout.columns, 1);
    DEFN_CHECK(layout.metrics.x + layout.metrics.width <= layout.battlefield.x);
    DEFN_CHECK(layout.tray.x >= layout.battlefield.x + layout.battlefield.width);
    DEFN_CHECK(layout.battlefield.height >= 220);
    DEFN_CHECK(layout.tray.width >= data.small_card.width);
    DEFN_CHECK(layout.tray_navigation_below);
}
DEFN_TEST(responsive_wide_pointer_window_retains_instruments_and_centered_cards) {
    const UiResponsiveData data;
    DisplaySnapshot display{{960, 540}};
    DEFN_CHECK(resolve_ui_profile(display, data, UiProfile::Small) == UiProfile::Standard);
    const auto layout = resolve_match_layout(display, data, data.standard_card, {1920, 1080}, 4, {.instruments_width = 900, .instruments_height = 64});
    DEFN_CHECK(layout.hud == HudArrangement::Instruments);
    DEFN_CHECK(layout.center_cards);
    DEFN_CHECK(layout.battlefield.height >= 460);
    display.fine_pointer = false;
    display.touch = true;
    DEFN_CHECK(resolve_ui_profile(display, data, UiProfile::Standard) == UiProfile::Small);
}
DEFN_TEST(display_comparison_and_context_cover_capability_changes_and_safe_area_ownership) {
    DisplaySnapshot first{{800, 300}, {24, 10, 8, 12}, false, 3, true, true, false};
    auto next = first;
    next.primary_coarse_pointer = true;
    DEFN_CHECK(first != next);
    const auto before = resolve_ui_context(first, {}, UiProfile::Small);
    const auto after = resolve_ui_context(next, {}, UiProfile::Small);
    DEFN_CHECK(!before.mobile_score && after.mobile_score);
    DEFN_CHECK_EQ(after.display.content.width, 768);
    DEFN_CHECK_EQ(after.display.content.height, 278);
    DEFN_CHECK(after.display.safe_area_applied);
    DEFN_CHECK_EQ(usable_rect(after.display).x, 0);
    DEFN_CHECK_EQ(usable_rect(after.display).width, 768);
    next = first;
    next.safe_area_applied = true;
    DEFN_CHECK(first != next);
    DEFN_CHECK_EQ(resolve_ui_context(next, {}, UiProfile::Small).display.content.width, 800);
    next = first;
    next.render_density += .00001F;
    next.content.width += .00001F;
    DEFN_CHECK(normalize_display_snapshot(first) == normalize_display_snapshot(next));
    next.content.width = std::numeric_limits<float>::quiet_NaN();
    next.safe_area.left = -10;
    next.render_density = std::numeric_limits<float>::infinity();
    const auto normalized = normalize_display_snapshot(next);
    DEFN_CHECK_EQ(normalized.content.width, 1);
    DEFN_CHECK_EQ(normalized.safe_area.left, 0);
    DEFN_CHECK_EQ(normalized.render_density, 1);
}
DEFN_TEST(tray_geometry_is_authoritative_at_exact_fit_and_overflow_boundaries) {
    const UiCardGeometry card{100, 48, 24};
    const auto fit = resolve_tray_layout({210, 48}, card, 10, 48, 2, false, false, true);
    DEFN_CHECK(!fit.overflow);
    DEFN_CHECK_EQ(fit.max_scroll, 0);
    const auto overflow = resolve_tray_layout({209, 48}, card, 10, 48, 2, false, false, true);
    DEFN_CHECK(overflow.overflow);
    DEFN_CHECK_EQ(overflow.clip.width, 113);
    DEFN_CHECK_EQ(overflow.max_scroll, 97);
    const auto tall = resolve_tray_layout({210, 100}, card, 10, 48, 5, true, true, false);
    DEFN_CHECK_EQ(tall.columns, 2);
    DEFN_CHECK(tall.overflow);
    DEFN_CHECK_EQ(tall.content.height, 164);
    DEFN_CHECK_EQ(tall.max_scroll, tall.content.height - tall.clip.height);
    const auto empty = resolve_tray_layout({210, 100}, card, 10, 48, 0, true, true, false);
    DEFN_CHECK(!empty.overflow);
    DEFN_CHECK_EQ(empty.max_scroll, 0);
}
} // namespace
