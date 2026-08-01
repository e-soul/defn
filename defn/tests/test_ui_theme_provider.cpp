// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "data_paths.h"
#include "ui_screen_scaffold.h"
#include "ui_sfx_player.h"
#include "ui_theme_loader.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

namespace {

Array color_array(double red, double green, double blue, double alpha) {
    Array values;
    values.append(red);
    values.append(green);
    values.append(blue);
    values.append(alpha);
    return values;
}

} // namespace

DEFN_TEST(ui_theme_loader_reads_the_shipped_theme_file) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());
    DEFN_CHECK(theme->find_surface("panel") != nullptr);
    DEFN_CHECK(theme->find_button("menu") != nullptr);
    DEFN_CHECK(theme->find_text_style("screen_title") != nullptr);
    DEFN_CHECK(!theme->sfx.hover.path.empty());
    DEFN_CHECK(!theme->sfx.click.path.empty());
}

DEFN_TEST(ui_theme_loader_returns_defaults_for_an_empty_dictionary) {
    const UiThemeData theme = UiThemeLoader::load_from_data(Dictionary());
    const UiThemeData defaults;
    DEFN_CHECK_EQ(theme.typography.body, defaults.typography.body);
    DEFN_CHECK_EQ(theme.spacing.xl, defaults.spacing.xl);
    DEFN_CHECK_EQ(theme.shape.corner_md, defaults.shape.corner_md);
    DEFN_CHECK(theme.surfaces.empty());
    DEFN_CHECK(theme.buttons.empty());
    DEFN_CHECK(theme.text_styles.empty());
}

DEFN_TEST(ui_theme_loader_merges_partial_data_over_defaults) {
    Dictionary palette;
    palette["accent"] = color_array(0.1, 0.2, 0.3, 1.0);

    Dictionary typography;
    typography["body"] = 21;

    Dictionary surface;
    surface["bg"] = "surface_sunken";
    surface["shape"] = "corner_lg";
    Dictionary surfaces;
    surfaces["card"] = surface;

    Dictionary normal;
    normal["bg"] = "accent";
    Array min_size;
    min_size.append(123);
    min_size.append(45);
    Dictionary button;
    button["min_size"] = min_size;
    button["normal"] = normal;
    Dictionary buttons;
    buttons["primary"] = button;

    Dictionary metrics;
    metrics["custom_width"] = 321;

    Dictionary data;
    data["palette"] = palette;
    data["typography"] = typography;
    data["surfaces"] = surfaces;
    data["buttons"] = buttons;
    data["metrics"] = metrics;

    const UiThemeData theme = UiThemeLoader::load_from_data(data);
    DEFN_CHECK_CLOSE(theme.palette.accent.r, 0.1F, 0.0001F);
    DEFN_CHECK_EQ(theme.typography.body, 21);
    DEFN_CHECK_EQ(theme.typography.title, UiThemeData().typography.title);

    const UiSurfaceStyle *card = theme.find_surface("card");
    DEFN_REQUIRE(card != nullptr);
    DEFN_CHECK_EQ(card->bg_role, std::string("surface_sunken"));
    DEFN_CHECK_EQ(card->border_role, std::string("border"));

    const UiButtonVariant *primary = theme.find_button("primary");
    DEFN_REQUIRE(primary != nullptr);
    DEFN_CHECK_EQ(primary->min_width, 123);
    DEFN_CHECK_EQ(primary->min_height, 45);
    DEFN_CHECK_EQ(primary->normal.bg_role, std::string("accent"));
    DEFN_CHECK_EQ(primary->hover.bg_role, std::string("accent"));

    DEFN_CHECK_EQ(theme.metric("custom_width"), 321);
}

DEFN_TEST(ui_theme_provider_memoises_the_generated_theme) {
    UiThemeProvider::reload();
    const Ref<Theme> first = UiThemeProvider::theme();
    const Ref<Theme> second = UiThemeProvider::theme();
    DEFN_REQUIRE(first.is_valid());
    DEFN_CHECK(first.ptr() == second.ptr());

    UiThemeProvider::reload();
    DEFN_CHECK(UiThemeProvider::theme().is_valid());
}

DEFN_TEST(ui_theme_provider_derives_variation_names_from_json_keys) {
    DEFN_CHECK_EQ(UiThemeProvider::label_variation("screen_title"), StringName("DefnScreenTitleLabel"));
    DEFN_CHECK_EQ(UiThemeProvider::button_variation("roster_selected"), StringName("DefnRosterSelectedButton"));
    DEFN_CHECK_EQ(UiThemeProvider::panel_variation("map_node"), StringName("DefnMapNodePanel"));
}

DEFN_TEST(ui_theme_provider_registers_variations_with_expected_base_types) {
    const Ref<Theme> theme = UiThemeProvider::theme();
    DEFN_REQUIRE(theme.is_valid());

    DEFN_CHECK(theme->is_type_variation("DefnMenuButton", "Button"));
    DEFN_CHECK(theme->is_type_variation("DefnScreenTitleLabel", "Label"));
    DEFN_CHECK(theme->is_type_variation("DefnPanelPanel", "PanelContainer"));
}

DEFN_TEST(ui_theme_provider_carries_json_shape_values_into_styleboxes) {
    const UiThemeData &data = UiThemeProvider::data();
    const UiButtonVariant *menu = data.find_button("menu");
    DEFN_REQUIRE(menu != nullptr);

    const Ref<Theme> theme = UiThemeProvider::theme();
    Ref<StyleBoxFlat> normal = theme->get_stylebox("normal", "DefnMenuButton");
    DEFN_REQUIRE(normal.is_valid());
    DEFN_CHECK_EQ(normal->get_corner_radius(CORNER_TOP_LEFT), data.find_shape_role(menu->shape_role).value_or(-1));
    DEFN_CHECK_EQ(normal->get_border_width(SIDE_TOP), data.shape.border_width);
}

DEFN_TEST(ui_theme_provider_preserves_the_original_score_screen_visual_contract) {
    const Ref<Theme> theme = UiThemeProvider::theme();

    const Ref<StyleBoxFlat> panel = theme->get_stylebox("panel", "DefnPanelPanel");
    DEFN_REQUIRE(panel.is_valid());
    DEFN_CHECK_CLOSE(panel->get_bg_color().r, 0.08F, 0.0001F);
    DEFN_CHECK_CLOSE(panel->get_bg_color().b, 0.14F, 0.0001F);
    DEFN_CHECK_CLOSE(panel->get_bg_color().a, 0.95F, 0.0001F);
    DEFN_CHECK_CLOSE(panel->get_border_color().r, 0.4F, 0.0001F);
    DEFN_CHECK_CLOSE(panel->get_border_color().b, 0.6F, 0.0001F);
    DEFN_CHECK_EQ(panel->get_corner_radius(CORNER_TOP_LEFT), 12);
    DEFN_CHECK_CLOSE(panel->get_content_margin(SIDE_LEFT), 32.0F, 0.0001F);

    const Ref<StyleBoxFlat> action_button = theme->get_stylebox("normal", "DefnSecondaryButton");
    DEFN_REQUIRE(action_button.is_valid());
    DEFN_CHECK_CLOSE(action_button->get_bg_color().r, 0.15F, 0.0001F);
    DEFN_CHECK_CLOSE(action_button->get_bg_color().b, 0.25F, 0.0001F);
    DEFN_CHECK_EQ(action_button->get_corner_radius(CORNER_TOP_LEFT), 8);
    DEFN_CHECK_CLOSE(action_button->get_content_margin(SIDE_LEFT), 12.0F, 0.0001F);
    DEFN_CHECK_CLOSE(action_button->get_content_margin(SIDE_RIGHT), 12.0F, 0.0001F);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnSecondaryButton"), 20);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnScoreStatLabel"), 22);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnMenuButton"), 32);
}

DEFN_TEST(ui_theme_provider_preserves_the_original_menu_visual_contract) {
    const Ref<Theme> theme = UiThemeProvider::theme();

    const Ref<StyleBoxFlat> normal = theme->get_stylebox("normal", "DefnMenuButton");
    DEFN_REQUIRE(normal.is_valid());
    DEFN_CHECK_CLOSE(normal->get_bg_color().r, 0.12F, 0.0001F);
    DEFN_CHECK_CLOSE(normal->get_bg_color().b, 0.18F, 0.0001F);
    DEFN_CHECK_CLOSE(normal->get_bg_color().a, 0.85F, 0.0001F);
    DEFN_CHECK_CLOSE(normal->get_border_color().r, 0.4F, 0.0001F);
    DEFN_CHECK_CLOSE(normal->get_border_color().b, 0.5F, 0.0001F);
    DEFN_CHECK_EQ(normal->get_border_width(SIDE_TOP), 2);
    DEFN_CHECK_EQ(normal->get_corner_radius(CORNER_TOP_LEFT), 8);

    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnMenuButton"), 32);
    DEFN_CHECK_CLOSE(theme->get_color("font_color", "DefnMenuButton").r, 0.9F, 0.0001F);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnOptionLabelLabel"), 24);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnOptionValueLabel"), 20);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnCareerScoreLabel"), 24);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnDeployCardTitleLabel"), 14);

    const UiThemeData &data = UiThemeProvider::data();
    const UiButtonVariant *menu = data.find_button("menu");
    DEFN_REQUIRE(menu != nullptr);
    DEFN_CHECK_EQ(menu->min_width, 400);
    DEFN_CHECK_EQ(menu->min_height, 60);
    DEFN_CHECK_EQ(data.metric("menu_button_separation", -1), 16);
    DEFN_CHECK_EQ(data.metric("deploy_card_horizontal_inset", -1), 8);
    DEFN_CHECK_EQ(data.metric("deploy_card_vertical_inset", -1), 6);
}

DEFN_TEST(ui_theme_provider_resolves_token_roles) {
    const UiThemeData &data = UiThemeProvider::data();
    DEFN_CHECK_EQ(UiThemeProvider::font_size("title"), data.typography.title);
    DEFN_CHECK_EQ(UiThemeProvider::spacing("xl"), data.spacing.xl);
    DEFN_CHECK_EQ(UiThemeProvider::shape("corner_lg"), data.shape.corner_lg);
    DEFN_CHECK_CLOSE(UiThemeProvider::color("accent").r, data.palette.accent.r, 0.0001F);
    DEFN_CHECK(UiThemeProvider::surface("panel").is_valid());
}

DEFN_TEST(ui_widgets_make_button_applies_theme_variation_and_min_size) {
    auto *button = make_button("Deploy", "menu");
    DEFN_REQUIRE(button != nullptr);

    const UiButtonVariant *menu = UiThemeProvider::data().find_button("menu");
    DEFN_REQUIRE(menu != nullptr);
    DEFN_CHECK_EQ(button->get_theme_type_variation(), StringName("DefnMenuButton"));
    DEFN_CHECK_EQ(static_cast<int>(button->get_custom_minimum_size().x), menu->min_width);
    DEFN_CHECK_EQ(static_cast<int>(button->get_custom_minimum_size().y), menu->min_height);
    DEFN_CHECK_EQ(button->get_text(), String("Deploy"));
    DEFN_CHECK(button->get_theme().ptr() == UiThemeProvider::theme().ptr());

    memdelete(button);
}

DEFN_TEST(ui_widgets_make_label_uses_variation_without_local_overrides) {
    auto *label = make_label("Career Score", "screen_title");
    DEFN_REQUIRE(label != nullptr);
    DEFN_CHECK_EQ(label->get_theme_type_variation(), StringName("DefnScreenTitleLabel"));
    DEFN_CHECK(!label->has_theme_color_override("font_color"));
    DEFN_CHECK(!label->has_theme_font_size_override("font_size"));
    DEFN_CHECK(label->get_theme().ptr() == UiThemeProvider::theme().ptr());

    memdelete(label);
}

DEFN_TEST(ui_widgets_apply_enabled_toggles_disabled_state) {
    auto *button = make_button("Back", "secondary");
    DEFN_REQUIRE(button != nullptr);

    apply_enabled(button, false);
    DEFN_CHECK(button->is_disabled());

    apply_enabled(button, true);
    DEFN_CHECK(!button->is_disabled());

    memdelete(button);
}

namespace {

bool scaffold_has_expected_chrome(Control *host, const UiScreenScaffold &scaffold) {
    return scaffold.root != nullptr && scaffold.header != nullptr && scaffold.body != nullptr && scaffold.footer != nullptr &&
           Object::cast_to<ColorRect>(scaffold.root) != nullptr && scaffold.root->get_parent() == host &&
           host->find_child("ScreenPanel", true, false) != nullptr && host->find_child("ScreenScroll", true, false) != nullptr &&
           scaffold.footer->get_alignment() == BoxContainer::ALIGNMENT_END;
}

bool scaffold_header_matches(const UiScreenScaffold &scaffold, const String &title_text, const String &subtitle_text) {
    auto *title = Object::cast_to<Label>(scaffold.header->get_child(0));
    auto *subtitle = Object::cast_to<Label>(scaffold.header->get_child(1));
    return title != nullptr && subtitle != nullptr && title->get_text() == title_text && subtitle->get_text() == subtitle_text &&
           title->get_theme_type_variation() == UiThemeProvider::label_variation(UiThemeProvider::data().screen.title_text_style);
}

} // namespace

DEFN_TEST(ui_screen_scaffold_builds_backdrop_header_body_and_right_aligned_footer) {
    auto *host = memnew(Control);
    const UiScreenScaffold scaffold = build_screen(host, {.title = "DEBRIEF", .subtitle = "Sector 4"});

    DEFN_REQUIRE(scaffold_has_expected_chrome(host, scaffold));
    DEFN_CHECK(scaffold_header_matches(scaffold, "DEBRIEF", "Sector 4"));

    memdelete(host);
}

DEFN_TEST(ui_screen_scaffold_honours_plain_root_and_supplied_content_size) {
    auto *host = memnew(Control);
    const UiScreenScaffold scaffold =
        build_screen(host, {.show_backdrop = false, .panelled_body = false, .scrollable_body = false, .max_content_size = godot::Vector2(640.0F, 480.0F)});

    DEFN_REQUIRE(scaffold.root != nullptr);
    DEFN_CHECK(Object::cast_to<ColorRect>(scaffold.root) == nullptr);
    DEFN_CHECK_EQ(scaffold.root->get_custom_minimum_size(), godot::Vector2(640.0F, 480.0F));
    DEFN_CHECK(host->find_child("ScreenPanel", true, false) == nullptr);
    DEFN_CHECK(host->find_child("ScreenScroll", true, false) == nullptr);
    DEFN_CHECK_EQ(scaffold.header->get_child_count(), 0);

    memdelete(host);
}

DEFN_TEST(ui_screen_scaffold_applies_theme_and_can_constrain_a_scrollable_panel) {
    auto *host = memnew(Control);
    const UiScreenScaffold scaffold = build_screen(
        host,
        {.title = "DEBRIEF", .panelled_body = true, .scrollable_body = true, .constrain_height = true, .max_content_size = godot::Vector2(720.0F, 560.0F)});

    DEFN_REQUIRE(scaffold.root != nullptr);
    DEFN_REQUIRE(scaffold.panel != nullptr);
    DEFN_CHECK(scaffold.root->get_theme().ptr() == UiThemeProvider::theme().ptr());
    DEFN_CHECK_EQ(scaffold.panel->get_custom_minimum_size(), godot::Vector2(720.0F, 560.0F));

    memdelete(host);
}

} // namespace defn
