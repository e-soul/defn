// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "data_paths.h"
#include "godot_string.h"
#include "ui_screen_scaffold.h"
#include "ui_sfx_player.h"
#include "ui_theme_loader.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <string>

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

/// A palette entry that borrows another role's colour at a different opacity.
Array reference_array(const char *role, double alpha) {
    Array values;
    values.append(String(role));
    values.append(alpha);
    return values;
}

/// Every campaign node state must resolve to a mark that actually ships, since the view no longer assembles the path itself.
void check_shipped_medallion(const UiThemeData &theme, const char *state) {
    const UiMedallionStyle *medallion = theme.find_medallion(state);
    DEFN_REQUIRE(medallion != nullptr);
    DEFN_CHECK(!medallion->mark.empty());
    DEFN_CHECK(ResourceLoader::get_singleton()->exists(to_godot_string(medallion->mark)));
}

/// A `<name>_selected` variant must be its base with the frame swapped: same footprint, same type, the shared
/// accent-and-heavier-border treatment, and a hover that still answers the cursor.
void check_selected_variant(const UiThemeData &theme, const char *name) {
    const UiButtonVariant *base = theme.find_button(name);
    DEFN_REQUIRE(base != nullptr);
    DEFN_REQUIRE(base->selected.has_value());

    const UiButtonVariant *selected = theme.find_button(std::string(name) + "_selected");
    DEFN_REQUIRE(selected != nullptr);
    DEFN_CHECK_EQ(selected->min_width, base->min_width);
    DEFN_CHECK_EQ(selected->min_height, base->min_height);
    DEFN_CHECK_EQ(selected->shape_role, base->shape_role);
    DEFN_CHECK_EQ(selected->content_margin_role, base->content_margin_role);
    DEFN_CHECK_EQ(selected->normal.border_role, std::string("accent"));
    DEFN_CHECK_EQ(selected->border_width_role, std::string("border_width_strong"));
    DEFN_CHECK(selected->hover.bg_role != selected->normal.bg_role);
    DEFN_CHECK(!selected->selected.has_value());
}

void check_role_resolves(const UiThemeData &theme, const char *role) { DEFN_CHECK(theme.find_color_role(role).has_value()); }

/// Both roles must resolve, and the first has to sit below the second on the neutral ramp. Comparing one
/// channel is enough because the ramp holds a single hue: every step moves all three the same way.
void check_lighter(const UiThemeData &theme, const char *darker, const char *lighter) {
    const auto low = theme.find_color_role(darker);
    const auto high = theme.find_color_role(lighter);
    DEFN_REQUIRE(low.has_value());
    DEFN_REQUIRE(high.has_value());
    DEFN_CHECK(low->r < high->r);
}

bool same_rgb(const Color &left, const Color &right) { return left.r == right.r && left.g == right.g && left.b == right.b; }

void check_distinct(const UiThemeData &theme, const char *role, const char *other) {
    const auto left = theme.find_color_role(role);
    const auto right = theme.find_color_role(other);
    DEFN_REQUIRE(left.has_value());
    DEFN_REQUIRE(right.has_value());
    DEFN_CHECK(!same_rgb(*left, *right));
}

void check_same(const UiThemeData &theme, const char *role, const char *other) {
    const auto left = theme.find_color_role(role);
    const auto right = theme.find_color_role(other);
    DEFN_REQUIRE(left.has_value());
    DEFN_REQUIRE(right.has_value());
    DEFN_CHECK(same_rgb(*left, *right));
}

/// A generated stylebox must carry the colour of the role its JSON entry names. Asserting against the role
/// rather than a literal keeps the check about the wiring, so re-tuning the palette does not break it while a
/// broken lookup still does.
void check_role_color(const godot::Color &actual, const char *role) {
    const auto expected = UiThemeProvider::data().find_color_role(role);
    DEFN_REQUIRE(expected.has_value());
    DEFN_CHECK_CLOSE(actual.r, expected->r, 0.0001F);
    DEFN_CHECK_CLOSE(actual.g, expected->g, 0.0001F);
    DEFN_CHECK_CLOSE(actual.b, expected->b, 0.0001F);
    DEFN_CHECK_CLOSE(actual.a, expected->a, 0.0001F);
}

void check_stylebox_bg(const Ref<StyleBoxFlat> &style, const char *role) { check_role_color(style->get_bg_color(), role); }

void check_stylebox_border(const Ref<StyleBoxFlat> &style, const char *role) { check_role_color(style->get_border_color(), role); }

void check_theme_color(const Ref<Theme> &theme, const char *type_name, const char *role) { check_role_color(theme->get_color("font_color", type_name), role); }

/// Same contract for the HUD instruments: a missing SVG would show as an empty rect over the battlefield.
void check_shipped_icon(const UiThemeData &theme, const char *key) {
    const UiMedallionStyle *icon = theme.find_icon(key);
    DEFN_REQUIRE(icon != nullptr);
    DEFN_CHECK(!icon->mark.empty());
    DEFN_CHECK(ResourceLoader::get_singleton()->exists(to_godot_string(icon->mark)));
    DEFN_CHECK(theme.find_color_role(icon->color_role).has_value());
}

} // namespace

DEFN_TEST(ui_theme_loader_reads_the_shipped_theme_file) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());
    DEFN_CHECK(theme->find_surface("panel") != nullptr);
    DEFN_CHECK(theme->find_button("menu") != nullptr);
    DEFN_CHECK(theme->find_text_style("screen_title") != nullptr);
    for (const char *state : {"available", "completed", "frontier", "locked"}) {
        check_shipped_medallion(*theme, state);
    }
    for (const char *key : {"energy", "integrity", "level", "score", "wave"}) {
        check_shipped_icon(*theme, key);
    }
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
    DEFN_CHECK(theme.medallions.empty());
    DEFN_CHECK(theme.icons.empty());
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

    Dictionary medallion;
    medallion["mark"] = "res://assets/ui/medallions/check.svg";
    medallion["color"] = "accent";
    Dictionary medallions;
    medallions["completed"] = medallion;

    Dictionary icon;
    icon["mark"] = "res://assets/ui/hud/bolt.svg";
    icon["color"] = "energy";
    Dictionary icons;
    icons["energy"] = icon;

    Dictionary metrics;
    metrics["custom_width"] = 321;

    Dictionary data;
    data["palette"] = palette;
    data["typography"] = typography;
    data["surfaces"] = surfaces;
    data["buttons"] = buttons;
    data["medallions"] = medallions;
    data["icons"] = icons;
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

    const UiMedallionStyle *completed = theme.find_medallion("completed");
    DEFN_REQUIRE(completed != nullptr);
    DEFN_CHECK_EQ(completed->mark, std::string("res://assets/ui/medallions/check.svg"));
    DEFN_CHECK_EQ(completed->color_role, std::string("accent"));

    const UiMedallionStyle *energy = theme.find_icon("energy");
    DEFN_REQUIRE(energy != nullptr);
    DEFN_CHECK_EQ(energy->mark, std::string("res://assets/ui/hud/bolt.svg"));
    DEFN_CHECK_EQ(energy->color_role, std::string("energy"));

    DEFN_CHECK_EQ(theme.metric("custom_width"), 321);
}

DEFN_TEST(ui_theme_loader_resolves_palette_references_to_their_ramp_entry) {
    Dictionary palette;
    palette["neutral_ink"] = color_array(0.02, 0.03, 0.05, 1.0);
    palette["text_inverse"] = "neutral_ink";
    palette["scrim"] = reference_array("neutral_ink", 0.72);
    // A chain still ends at the literal, and the nearest opacity on the way is the one that survives.
    palette["deep_scrim"] = reference_array("scrim", 0.9);
    palette["borrowed_scrim"] = "scrim";

    Dictionary data;
    data["palette"] = palette;
    const UiThemeData theme = UiThemeLoader::load_from_data(data);

    const auto inverse = theme.find_color_role("text_inverse");
    DEFN_REQUIRE(inverse.has_value());
    DEFN_CHECK_CLOSE(inverse->r, 0.02F, 0.0001F);
    DEFN_CHECK_CLOSE(inverse->a, 1.0F, 0.0001F);
    // The named field and the role lookup must agree; the field is what the fallback palette exposes.
    DEFN_CHECK_CLOSE(theme.palette.text_inverse.b, 0.05F, 0.0001F);

    const auto scrim = theme.find_color_role("scrim");
    DEFN_REQUIRE(scrim.has_value());
    DEFN_CHECK_CLOSE(scrim->g, 0.03F, 0.0001F);
    DEFN_CHECK_CLOSE(scrim->a, 0.72F, 0.0001F);

    const auto deep_scrim = theme.find_color_role("deep_scrim");
    DEFN_REQUIRE(deep_scrim.has_value());
    DEFN_CHECK_CLOSE(deep_scrim->r, 0.02F, 0.0001F);
    DEFN_CHECK_CLOSE(deep_scrim->a, 0.9F, 0.0001F);

    const auto borrowed = theme.find_color_role("borrowed_scrim");
    DEFN_REQUIRE(borrowed.has_value());
    DEFN_CHECK_CLOSE(borrowed->a, 0.72F, 0.0001F);
}

DEFN_TEST(ui_theme_loader_drops_palette_references_that_lead_nowhere) {
    Dictionary palette;
    palette["accent"] = color_array(0.9, 0.8, 0.3, 1.0);
    palette["dangling"] = "no_such_role";
    palette["loop_a"] = "loop_b";
    palette["loop_b"] = "loop_a";

    Dictionary data;
    data["palette"] = palette;
    const UiThemeData theme = UiThemeLoader::load_from_data(data);

    // Leaving a broken role out is what lets ContentValidator name every place that references it, instead of
    // the game rendering a silent black where a colour was meant to be.
    DEFN_CHECK(theme.find_color_role("accent").has_value());
    DEFN_CHECK(!theme.find_color_role("dangling").has_value());
    DEFN_CHECK(!theme.find_color_role("loop_a").has_value());
    DEFN_CHECK(!theme.find_color_role("loop_b").has_value());
}

DEFN_TEST(ui_theme_shipped_palette_resolves_every_semantic_role) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());

    // The semantic names the rest of the theme is written against all borrow from the ramp; a typo in one of
    // those references would silently remove the role rather than fail the parse.
    for (const char *role : {"surface", "surface_raised", "surface_sunken", "surface_elevated", "border", "border_muted", "border_strong", "border_focus",
                             "text_primary", "text_secondary", "text_muted", "text_disabled", "text_inverse", "text_outline", "victory", "defeat",
                             "integrity_critical", "backdrop", "overlay_scrim"}) {
        check_role_resolves(*theme, role);
    }
}

DEFN_TEST(ui_theme_shipped_palette_orders_the_elevation_ladder) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());

    // Elevation only means anything if the ladder is ordered, and it was not before the ramp landed:
    // `surface_sunken` used to hold the same value as `surface`.
    check_lighter(*theme, "surface_sunken", "surface");
    check_lighter(*theme, "surface", "surface_raised");
    check_lighter(*theme, "surface_raised", "surface_elevated");
}

DEFN_TEST(ui_theme_shipped_palette_separates_focus_from_a_strong_border) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());

    // Focus has to be distinguishable from an ordinary strong border to be worth styling at all.
    check_distinct(*theme, "border_focus", "border_strong");
    // The two greens and the two reds the palette used to carry are now one of each.
    check_same(*theme, "victory", "state_success");
    check_same(*theme, "defeat", "state_danger");
    check_same(*theme, "integrity_critical", "state_danger");
}

DEFN_TEST(ui_theme_provider_replaces_the_marks_godot_controls_draw_for_themselves) {
    const Ref<Theme> theme = UiThemeProvider::theme();
    DEFN_REQUIRE(theme.is_valid());

    // Styleboxes cannot reach these; left alone they render in the engine's stock greys.
    for (const char *icon : {"grabber", "grabber_highlight", "grabber_disabled"}) {
        DEFN_CHECK(theme->get_icon(icon, "HSlider").is_valid());
    }
    for (const char *icon : {"radio_checked", "radio_unchecked", "checked", "unchecked"}) {
        DEFN_CHECK(theme->get_icon(icon, "PopupMenu").is_valid());
    }
    DEFN_CHECK_EQ(theme->get_constant("modulate_arrow", "OptionButton"), 1);
}

DEFN_TEST(ui_theme_control_icons_are_tinted_to_their_role) {
    const auto theme_data = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme_data.has_value());

    const UiMedallionStyle *knob = theme_data->find_control_icon("slider_knob");
    DEFN_REQUIRE(knob != nullptr);
    DEFN_CHECK(!knob->mark.empty());
    DEFN_CHECK(ResourceLoader::get_singleton()->exists(to_godot_string(knob->mark)));

    const auto expected = theme_data->find_color_role(knob->color_role);
    DEFN_REQUIRE(expected.has_value());

    const Ref<Texture2D> texture = UiThemeProvider::theme()->get_icon("grabber", "HSlider");
    DEFN_REQUIRE(texture.is_valid());
    const Ref<Image> image = texture->get_image();
    DEFN_REQUIRE(image.is_valid());

    // The mark ships white; what reaches the screen has to be the role the theme named for it.
    const godot::Color centre = image->get_pixel(image->get_width() / 2, image->get_height() / 2);
    DEFN_CHECK_CLOSE(centre.r, expected->r, 0.02F);
    DEFN_CHECK_CLOSE(centre.g, expected->g, 0.02F);
    DEFN_CHECK_CLOSE(centre.b, expected->b, 0.02F);
}

DEFN_TEST(ui_theme_derives_a_selected_variant_from_every_variant_that_declares_one) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());

    for (const char *name : {"card", "roster"}) {
        check_selected_variant(*theme, name);
    }
}

DEFN_TEST(ui_theme_provider_honours_a_variant_border_width) {
    const Ref<Theme> theme = UiThemeProvider::theme();
    DEFN_REQUIRE(theme.is_valid());
    const UiThemeData &data = UiThemeProvider::data();

    const Ref<StyleBoxFlat> plain = theme->get_stylebox("normal", "DefnCardButton");
    const Ref<StyleBoxFlat> selected = theme->get_stylebox("normal", "DefnCardSelectedButton");
    DEFN_REQUIRE(plain.is_valid());
    DEFN_REQUIRE(selected.is_valid());
    DEFN_CHECK_EQ(plain->get_border_width(SIDE_TOP), data.shape.border_width);
    DEFN_CHECK_EQ(selected->get_border_width(SIDE_TOP), data.shape.border_width_strong);
}

DEFN_TEST(ui_theme_names_its_transition_lengths) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());

    // Three ordered steps, so no two animations in the game drift onto durations of their own.
    DEFN_CHECK(theme->motion.fast < theme->motion.base);
    DEFN_CHECK(theme->motion.base < theme->motion.slow);
    DEFN_CHECK_CLOSE(UiThemeProvider::motion("fast"), theme->motion.fast, 0.0001F);
    // An unknown name falls back to the middle step rather than to zero, which would read as no animation.
    DEFN_CHECK_CLOSE(UiThemeProvider::motion("no_such_step"), theme->motion.base, 0.0001F);
}

DEFN_TEST(ui_theme_separates_focus_from_selection_and_emphasis) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());

    // Focus is now reachable, so it needs a colour that is neither an ordinary strong border nor the accent a
    // selected card wears.
    check_distinct(*theme, "border_focus", "border_strong");
    check_distinct(*theme, "border_focus", "accent");
}

DEFN_TEST(ui_theme_gives_the_deploy_card_its_own_press_sound) {
    const auto theme = UiThemeLoader::load(DataPaths::UI_THEME);
    DEFN_REQUIRE(theme.has_value());

    const UiButtonVariant *deploy = theme->find_button("deploy_card");
    const UiButtonVariant *secondary = theme->find_button("secondary");
    DEFN_REQUIRE(deploy != nullptr);
    DEFN_REQUIRE(secondary != nullptr);
    DEFN_CHECK_EQ(deploy->sfx_role, std::string("deploy_card"));
    DEFN_CHECK_EQ(secondary->sfx_role, std::string("click"));
    DEFN_CHECK(!theme->sfx.press("deploy_card").path.empty());
    DEFN_CHECK_EQ(theme->sfx.press("click").path, theme->sfx.click.path);
}

DEFN_TEST(ui_widgets_make_button_takes_focus) {
    auto *button = make_button("Deploy", "menu");
    DEFN_REQUIRE(button != nullptr);
    // The game is navigable by keyboard and pad; every button the factory builds participates.
    DEFN_CHECK_EQ(button->get_focus_mode(), Control::FOCUS_ALL);
    memdelete(button);
}

DEFN_TEST(ui_widgets_make_card_leaves_a_display_only_card_inert) {
    const CardNodes on_offer = make_card({.variant = "card"});
    const CardNodes on_display = make_card({.variant = "card", .interactive = false});
    DEFN_REQUIRE(on_offer.button != nullptr);
    DEFN_REQUIRE(on_display.button != nullptr);

    // An owned upgrade is shown, not offered: it should not hover, click or take focus as though it were.
    DEFN_CHECK_EQ(on_offer.button->get_mouse_filter(), Control::MOUSE_FILTER_STOP);
    DEFN_CHECK_EQ(on_display.button->get_mouse_filter(), Control::MOUSE_FILTER_IGNORE);
    DEFN_CHECK_EQ(on_display.button->get_focus_mode(), Control::FOCUS_NONE);

    memdelete(on_offer.button);
    memdelete(on_display.button);
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

DEFN_TEST(ui_theme_provider_wires_the_score_screen_chrome_to_its_roles) {
    const Ref<Theme> theme = UiThemeProvider::theme();

    const Ref<StyleBoxFlat> panel = theme->get_stylebox("panel", "DefnPanelPanel");
    DEFN_REQUIRE(panel.is_valid());
    check_stylebox_bg(panel, "surface");
    check_stylebox_border(panel, "border");
    DEFN_CHECK_EQ(panel->get_corner_radius(CORNER_TOP_LEFT), 12);
    DEFN_CHECK_CLOSE(panel->get_content_margin(SIDE_LEFT), 32.0F, 0.0001F);

    const Ref<StyleBoxFlat> action_button = theme->get_stylebox("normal", "DefnSecondaryButton");
    DEFN_REQUIRE(action_button.is_valid());
    check_stylebox_bg(action_button, "surface_raised");
    DEFN_CHECK_EQ(action_button->get_corner_radius(CORNER_TOP_LEFT), 8);
    DEFN_CHECK_CLOSE(action_button->get_content_margin(SIDE_LEFT), 12.0F, 0.0001F);
    DEFN_CHECK_CLOSE(action_button->get_content_margin(SIDE_RIGHT), 12.0F, 0.0001F);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnSecondaryButton"), 20);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnScoreStatLabel"), 22);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnMenuButton"), 32);
}

DEFN_TEST(ui_theme_provider_wires_the_menu_chrome_to_its_roles) {
    const Ref<Theme> theme = UiThemeProvider::theme();

    const Ref<StyleBoxFlat> normal = theme->get_stylebox("normal", "DefnMenuButton");
    DEFN_REQUIRE(normal.is_valid());
    check_stylebox_bg(normal, "surface_raised");
    check_stylebox_border(normal, "border");
    DEFN_CHECK_EQ(normal->get_border_width(SIDE_TOP), 2);
    DEFN_CHECK_EQ(normal->get_corner_radius(CORNER_TOP_LEFT), 8);

    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnMenuButton"), 32);
    check_theme_color(theme, "DefnMenuButton", "text_primary");
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnOptionLabelLabel"), 24);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnOptionValueLabel"), 20);
    // The menu's career score is an instrument readout now, wearing the same styles as the HUD score plate.
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnHudScoreLabel"), 22);
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnHudLabelLabel"), 13);
    // Deploy cards, roster chips and upgrade cards all name their card through one title style.
    DEFN_CHECK_EQ(theme->get_font_size("font_size", "DefnCardTitleLabel"), 15);

    const UiThemeData &data = UiThemeProvider::data();
    const UiButtonVariant *menu = data.find_button("menu");
    DEFN_REQUIRE(menu != nullptr);
    DEFN_CHECK_EQ(menu->min_width, 400);
    DEFN_CHECK_EQ(menu->min_height, 60);
    DEFN_CHECK_EQ(data.metric("menu_button_separation", -1), 16);
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
