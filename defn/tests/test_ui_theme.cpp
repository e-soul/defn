// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"
#include "ui_theme_models.h"

namespace {

using namespace defn;

UiThemeData make_theme() {
    UiThemeData theme;
    theme.surfaces.emplace("panel", UiSurfaceStyle{.bg_role = "surface_raised", .border_role = "border_strong", .shape_role = "corner_lg"});
    theme.buttons.emplace("menu", UiButtonVariant{.min_width = 400, .min_height = 60, .font_size_role = "heading"});
    theme.text_styles.emplace("screen_title", UiTextStyle{.font_size_role = "title", .color_role = "accent"});
    theme.medallions.emplace("completed", UiMedallionStyle{.mark = "res://assets/ui/medallions/check.svg", .color_role = "state_success"});
    theme.hud_icons.emplace("energy", UiMedallionStyle{.mark = "res://assets/ui/hud/bolt.svg", .color_role = "energy"});
    theme.metrics.emplace("option_label_width", 250);
    return theme;
}

DEFN_TEST(ui_theme_defaults_are_populated) {
    const UiThemeData theme;
    DEFN_CHECK_EQ(theme.typography.body, 18);
    DEFN_CHECK_EQ(theme.spacing.section_gap, 16);
    DEFN_CHECK_EQ(theme.shape.corner_md, 8);
    DEFN_CHECK_EQ(theme.screen.panel_surface, std::string("panel"));
    DEFN_CHECK(theme.surfaces.empty());
    DEFN_CHECK(theme.buttons.empty());
    DEFN_CHECK(theme.text_styles.empty());
    DEFN_CHECK(theme.medallions.empty());
    DEFN_CHECK(theme.hud_icons.empty());
    // The medallion default must stay a neutral role: views fall back to it when a state has no entry.
    DEFN_CHECK_EQ(UiMedallionStyle().color_role, std::string("text_primary"));
}

DEFN_TEST(ui_theme_named_lookups_resolve_registered_entries) {
    const UiThemeData theme = make_theme();

    const UiSurfaceStyle *surface = theme.find_surface("panel");
    DEFN_REQUIRE(surface != nullptr);
    DEFN_CHECK_EQ(surface->bg_role, std::string("surface_raised"));

    const UiButtonVariant *button = theme.find_button("menu");
    DEFN_REQUIRE(button != nullptr);
    DEFN_CHECK_EQ(button->min_width, 400);

    const UiTextStyle *text_style = theme.find_text_style("screen_title");
    DEFN_REQUIRE(text_style != nullptr);
    DEFN_CHECK_EQ(text_style->color_role, std::string("accent"));

    const UiMedallionStyle *medallion = theme.find_medallion("completed");
    DEFN_REQUIRE(medallion != nullptr);
    DEFN_CHECK_EQ(medallion->mark, std::string("res://assets/ui/medallions/check.svg"));
    DEFN_CHECK_EQ(medallion->color_role, std::string("state_success"));

    // HUD icons share the medallion shape but live in their own namespace, so the two must not resolve each other.
    const UiMedallionStyle *hud_icon = theme.find_hud_icon("energy");
    DEFN_REQUIRE(hud_icon != nullptr);
    DEFN_CHECK_EQ(hud_icon->mark, std::string("res://assets/ui/hud/bolt.svg"));
    DEFN_CHECK_EQ(hud_icon->color_role, std::string("energy"));
    DEFN_CHECK(theme.find_medallion("energy") == nullptr);
    DEFN_CHECK(theme.find_hud_icon("completed") == nullptr);
}

DEFN_TEST(ui_theme_named_lookups_return_null_for_unknown_entries) {
    const UiThemeData theme = make_theme();
    DEFN_CHECK(theme.find_surface("nope") == nullptr);
    DEFN_CHECK(theme.find_button("nope") == nullptr);
    DEFN_CHECK(theme.find_text_style("nope") == nullptr);
    DEFN_CHECK(theme.find_medallion("nope") == nullptr);
    DEFN_CHECK(theme.find_hud_icon("nope") == nullptr);
}

DEFN_TEST(ui_theme_role_lookups_resolve_tokens) {
    const UiThemeData theme = make_theme();

    const auto accent = theme.find_color_role("accent");
    DEFN_REQUIRE(accent.has_value());
    DEFN_CHECK_CLOSE(accent->r, theme.palette.accent.r, 0.0001F);

    const auto title = theme.find_font_size_role("title");
    DEFN_REQUIRE(title.has_value());
    DEFN_CHECK_EQ(*title, theme.typography.title);

    const auto gap = theme.find_spacing_role("section_gap");
    DEFN_REQUIRE(gap.has_value());
    DEFN_CHECK_EQ(*gap, theme.spacing.section_gap);

    const auto corner = theme.find_shape_role("corner_lg");
    DEFN_REQUIRE(corner.has_value());
    DEFN_CHECK_EQ(*corner, theme.shape.corner_lg);
}

DEFN_TEST(ui_theme_role_lookups_reject_unknown_roles) {
    const UiThemeData theme = make_theme();
    DEFN_CHECK(!theme.find_color_role("chartreuse").has_value());
    DEFN_CHECK(!theme.find_font_size_role("gigantic").has_value());
    DEFN_CHECK(!theme.find_spacing_role("xxl").has_value());
    DEFN_CHECK(!theme.find_shape_role("corner_xl").has_value());
}

DEFN_TEST(ui_theme_metrics_fall_back_when_missing) {
    const UiThemeData theme = make_theme();
    DEFN_CHECK_EQ(theme.metric("option_label_width"), 250);
    DEFN_CHECK_EQ(theme.metric("unknown_metric", 42), 42);
    DEFN_CHECK_EQ(theme.metric("unknown_metric"), 0);
}

} // namespace
