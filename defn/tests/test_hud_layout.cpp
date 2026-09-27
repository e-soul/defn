// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud_layout.h"
#include "test_harness.h"

namespace defn {

namespace {

bool overlaps(const HudRect &left, const HudRect &right) {
    return left.x < right.x + right.width && left.x + left.width > right.x && left.y < right.y + right.height && left.y + left.height > right.y;
}

UiViewportMetrics phone_landscape() {
    return {.width = 2400.0F, .height = 1080.0F, .css_width = 844.0F, .css_height = 380.0F, .web = true, .coarse_pointer = true};
}

} // namespace

DEFN_TEST(hud_sizing_preserves_roomy_desktop_and_native_defaults) {
    DEFN_CHECK(!resolve_hud_sizing({}).responsive);
    DEFN_CHECK(!resolve_hud_sizing({.web = true}).responsive);
    DEFN_CHECK(!resolve_hud_sizing({.width = 390.0F, .height = 844.0F, .css_width = 390.0F, .css_height = 844.0F}).responsive);
    DEFN_CHECK(!resolve_hud_sizing({.css_width = 0.0F, .web = true}).responsive);
}

DEFN_TEST(hud_sizing_uses_displayed_pixels_and_readability_floors) {
    const auto viewport = phone_landscape();
    const HudSizing sizing = resolve_hud_sizing(viewport);
    DEFN_CHECK(sizing.responsive);
    DEFN_CHECK(!sizing.compact);
    DEFN_CHECK(!sizing.wrap_cards);
    DEFN_CHECK(sizing.readout_scale > 1.3F);
    DEFN_CHECK_CLOSE(sizing.card_height * sizing.pixels_per_unit, 44.0F, 0.001F);
    DEFN_CHECK_CLOSE(sizing.portrait_size * sizing.pixels_per_unit, 28.0F, 0.001F);
    DEFN_CHECK_CLOSE(sizing.pause_height * sizing.pixels_per_unit, 44.0F, 0.001F);

    // A different render/UI coordinate density gives the same displayed card and font sizes.
    auto doubled = viewport;
    doubled.width *= 2.0F;
    doubled.height *= 2.0F;
    const HudSizing other = resolve_hud_sizing(doubled);
    DEFN_CHECK_CLOSE(sizing.card_height * sizing.pixels_per_unit, other.card_height * other.pixels_per_unit, 0.001F);
    DEFN_CHECK_CLOSE(sizing.card_width * sizing.pixels_per_unit, other.card_width * other.pixels_per_unit, 0.001F);
}

DEFN_TEST(hud_top_plates_reflow_below_fullscreen_chrome_and_clear_pause) {
    auto viewport = phone_landscape();
    viewport.overlay_width = 120.0F;
    viewport.overlay_height = 48.0F;
    const HudSizing sizing = resolve_hud_sizing(viewport);
    const HudPlateSizes plates{
        .energy = {.width = 340.0F, .height = 100.0F}, .info = {.width = 1200.0F, .height = 100.0F}, .integrity = {.width = 430.0F, .height = 100.0F}};
    const HudPlacement placed = place_hud(viewport, sizing, plates, 1800.0F, sizing.card_height);
    const HudRect chrome{.x = viewport.width - viewport.overlay_width / sizing.pixels_per_unit,
                         .width = viewport.overlay_width / sizing.pixels_per_unit,
                         .height = viewport.overlay_height / sizing.pixels_per_unit};
    DEFN_CHECK(!overlaps(placed.energy, placed.info));
    DEFN_CHECK(!overlaps(placed.info, placed.integrity));
    DEFN_CHECK(!overlaps(placed.integrity, chrome));
    DEFN_CHECK(!overlaps(placed.info, chrome));
    DEFN_CHECK(!overlaps(placed.tray, placed.pause));
    DEFN_CHECK(placed.info.y >= chrome.height);
}

DEFN_TEST(hud_portrait_roster_wraps_without_shrinking_card_targets) {
    const UiViewportMetrics viewport{.width = 1920.0F, .height = 3900.0F, .css_width = 390.0F, .css_height = 792.0F, .web = true, .coarse_pointer = true};
    const HudSizing sizing = resolve_hud_sizing(viewport);
    DEFN_CHECK(sizing.compact);
    DEFN_CHECK(sizing.wrap_cards);
    const float available = viewport.width - (2.0F * sizing.margin) - sizing.pause_width - sizing.gap;
    const float units = 1.0F / sizing.pixels_per_unit;
    const HudRect cards[] = {{.width = 140.0F * units, .height = sizing.card_height},
                             {.width = 145.0F * units, .height = sizing.card_height},
                             {.width = 120.0F * units, .height = sizing.card_height},
                             {.width = 140.0F * units, .height = sizing.card_height}};
    const auto arrangement = arrange_hud_cards(cards, available, sizing.gap, true);
    const HudPlacement placed = place_hud(viewport, sizing, {}, arrangement.width, arrangement.height);
    DEFN_CHECK(!placed.scroll_cards);
    DEFN_CHECK(!overlaps(placed.tray, placed.pause));
    DEFN_CHECK(placed.tray.x >= sizing.margin);
    DEFN_CHECK(placed.tray.x + placed.tray.width <= viewport.width - sizing.margin);
    DEFN_CHECK_CLOSE(placed.tray.height * sizing.pixels_per_unit, 94.0F, 0.001F);
    DEFN_CHECK_CLOSE(arrangement.cards[0].y, arrangement.cards[1].y, 0.001F);
    DEFN_CHECK_CLOSE(arrangement.cards[2].y, arrangement.cards[3].y, 0.001F);
    DEFN_CHECK(arrangement.cards[2].y > arrangement.cards[0].y);
    DEFN_CHECK_CLOSE(arrangement.cards[2].x, 0.0F, 0.001F);
}

DEFN_TEST(hud_card_rows_keep_order_and_landscape_overflow_available_for_scrolling) {
    const HudRect cards[] = {{.width = 150.0F, .height = 44.0F}, {.width = 140.0F, .height = 44.0F}, {.width = 120.0F, .height = 44.0F}};
    const auto row = arrange_hud_cards(cards, 300.0F, 6.0F, false);
    DEFN_CHECK_CLOSE(row.width, 422.0F, 0.001F);
    DEFN_CHECK_CLOSE(row.height, 44.0F, 0.001F);
    DEFN_CHECK_CLOSE(row.cards[2].x, 302.0F, 0.001F);
    DEFN_CHECK(arrange_hud_cards({}, 300.0F, 6.0F, true).cards.empty());
}

DEFN_TEST(hud_top_plates_share_a_row_and_height_when_all_readings_fit) {
    auto viewport = phone_landscape();
    viewport.overlay_width = 120.0F;
    viewport.overlay_height = 48.0F;
    const auto sizing = resolve_hud_sizing(viewport);
    const HudPlateSizes plates{
        .energy = {.width = 330.0F, .height = 95.0F}, .info = {.width = 1000.0F, .height = 110.0F}, .integrity = {.width = 620.0F, .height = 105.0F}};
    const auto placed = place_hud(viewport, sizing, plates, 0.0F, 0.0F);
    DEFN_CHECK_CLOSE(placed.energy.y, placed.info.y, 0.001F);
    DEFN_CHECK_CLOSE(placed.integrity.y, placed.info.y, 0.001F);
    DEFN_CHECK_CLOSE(placed.energy.height, placed.info.height, 0.001F);
    DEFN_CHECK_CLOSE(placed.integrity.height, placed.info.height, 0.001F);
    DEFN_CHECK(!overlaps(placed.energy, placed.info));
    DEFN_CHECK(!overlaps(placed.info, placed.integrity));
    DEFN_CHECK(placed.integrity.x + placed.integrity.width + sizing.gap <= viewport.width - sizing.margin - viewport.overlay_width / sizing.pixels_per_unit);
}

DEFN_TEST(hud_mobile_card_dimensions_accept_theme_data_and_preserve_touch_height) {
    const auto viewport = phone_landscape();
    const auto sizing = resolve_hud_sizing(viewport, {.width = 200.0F, .height = 48.0F, .portrait_size = 32.0F, .inset = 6.0F});
    DEFN_CHECK_CLOSE(sizing.card_width * sizing.pixels_per_unit, 200.0F, 0.001F);
    DEFN_CHECK_CLOSE(sizing.card_height * sizing.pixels_per_unit, 48.0F, 0.001F);
    DEFN_CHECK_CLOSE(sizing.portrait_size * sizing.pixels_per_unit, 32.0F, 0.001F);
    const auto small = resolve_hud_sizing(viewport, {.height = 20.0F, .portrait_size = 80.0F});
    DEFN_CHECK_CLOSE(small.card_height * small.pixels_per_unit, 44.0F, 0.001F);
    DEFN_CHECK(small.portrait_size <= small.card_height - (2.0F * small.card_inset));
}

DEFN_TEST(hud_layout_keeps_a_small_roster_centered_when_pause_does_not_obstruct_it) {
    const auto viewport = phone_landscape();
    const auto sizing = resolve_hud_sizing(viewport);
    const auto placed = place_hud(viewport, sizing, {}, sizing.card_width, sizing.card_height);
    DEFN_CHECK(!placed.scroll_cards);
    DEFN_CHECK_CLOSE(placed.tray.x + placed.tray.width * 0.5F, viewport.width * 0.5F, 0.001F);
    DEFN_CHECK(!overlaps(placed.tray, placed.pause));
}

DEFN_TEST(hud_wide_web_layout_converts_fullscreen_chrome_even_without_mobile_enlargement) {
    const UiViewportMetrics viewport{.css_width = 1280.0F, .css_height = 720.0F, .overlay_width = 120.0F, .overlay_height = 48.0F, .web = true};
    const HudSizing sizing = resolve_hud_sizing(viewport);
    DEFN_CHECK(!sizing.responsive);
    DEFN_CHECK_CLOSE(sizing.pixels_per_unit, 2.0F / 3.0F, 0.001F);
    const HudPlacement placed = place_hud(viewport, sizing, {.integrity = {.width = 230.0F, .height = 64.0F}}, 0.0F, 0.0F);
    DEFN_CHECK((placed.integrity.x + placed.integrity.width) * sizing.pixels_per_unit <= viewport.css_width - viewport.overlay_width);
}

} // namespace defn
