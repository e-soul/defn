// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_theme_loader.h"

#include "godot_string.h"
#include "json_file_loader.h"
#include "variant_tools.h"

#include <godot_cpp/variant/array.hpp>

namespace defn {

using namespace godot;

namespace {

constexpr float DEFAULT_ALPHA = 1.0F;

Color parse_color(const Array &values, const Color &fallback) {
    if (values.size() >= 3) {
        return {
            .r = VariantTools::as_float(values[0]),
            .g = VariantTools::as_float(values[1]),
            .b = VariantTools::as_float(values[2]),
            .a = values.size() >= 4 ? VariantTools::as_float(values[3]) : DEFAULT_ALPHA,
        };
    }
    return fallback;
}

std::string parse_role(const Dictionary &source, const char *key, const std::string &fallback) {
    const String value = source.get(key, "");
    return value.is_empty() ? fallback : to_std_string(value);
}

UiPalette parse_palette(const Dictionary &source, UiPalette palette) {
    palette.surface = parse_color(source.get("surface", Array()), palette.surface);
    palette.surface_raised = parse_color(source.get("surface_raised", Array()), palette.surface_raised);
    palette.surface_sunken = parse_color(source.get("surface_sunken", Array()), palette.surface_sunken);
    palette.overlay_scrim = parse_color(source.get("overlay_scrim", Array()), palette.overlay_scrim);
    palette.border = parse_color(source.get("border", Array()), palette.border);
    palette.border_strong = parse_color(source.get("border_strong", Array()), palette.border_strong);
    palette.border_focus = parse_color(source.get("border_focus", Array()), palette.border_focus);
    palette.accent = parse_color(source.get("accent", Array()), palette.accent);
    palette.accent_strong = parse_color(source.get("accent_strong", Array()), palette.accent_strong);
    palette.text_primary = parse_color(source.get("text_primary", Array()), palette.text_primary);
    palette.text_secondary = parse_color(source.get("text_secondary", Array()), palette.text_secondary);
    palette.text_muted = parse_color(source.get("text_muted", Array()), palette.text_muted);
    palette.text_inverse = parse_color(source.get("text_inverse", Array()), palette.text_inverse);
    palette.state_success = parse_color(source.get("state_success", Array()), palette.state_success);
    palette.state_warning = parse_color(source.get("state_warning", Array()), palette.state_warning);
    palette.state_danger = parse_color(source.get("state_danger", Array()), palette.state_danger);
    palette.state_locked = parse_color(source.get("state_locked", Array()), palette.state_locked);
    palette.energy = parse_color(source.get("energy", Array()), palette.energy);
    palette.victory = parse_color(source.get("victory", Array()), palette.victory);
    palette.defeat = parse_color(source.get("defeat", Array()), palette.defeat);
    palette.overlay_victory = parse_color(source.get("overlay_victory", Array()), palette.overlay_victory);
    palette.overlay_defeat = parse_color(source.get("overlay_defeat", Array()), palette.overlay_defeat);
    palette.backdrop = parse_color(source.get("backdrop", Array()), palette.backdrop);
    palette.scrim_soft = parse_color(source.get("scrim_soft", Array()), palette.scrim_soft);
    palette.scrim_panel = parse_color(source.get("scrim_panel", Array()), palette.scrim_panel);
    palette.route_locked = parse_color(source.get("route_locked", Array()), palette.route_locked);
    palette.locked_tint = parse_color(source.get("locked_tint", Array()), palette.locked_tint);
    palette.ambience_dust = parse_color(source.get("ambience_dust", Array()), palette.ambience_dust);
    palette.ambience_spores = parse_color(source.get("ambience_spores", Array()), palette.ambience_spores);
    palette.ambience_mist = parse_color(source.get("ambience_mist", Array()), palette.ambience_mist);
    palette.ambience_snow = parse_color(source.get("ambience_snow", Array()), palette.ambience_snow);
    palette.ambience_embers = parse_color(source.get("ambience_embers", Array()), palette.ambience_embers);
    palette.transparent = parse_color(source.get("transparent", Array()), palette.transparent);

    const Array keys = source.keys();
    for (const Variant &raw_key : keys) {
        const String key = raw_key;
        palette.extra[to_std_string(key)] = parse_color(source.get(key, Array()), Color{});
    }
    return palette;
}

UiTypography parse_typography(const Dictionary &source, UiTypography typography) {
    typography.banner = VariantTools::as_int(source.get("banner", typography.banner));
    typography.display = VariantTools::as_int(source.get("display", typography.display));
    typography.title = VariantTools::as_int(source.get("title", typography.title));
    typography.menu = VariantTools::as_int(source.get("menu", typography.menu));
    typography.section = VariantTools::as_int(source.get("section", typography.section));
    typography.heading = VariantTools::as_int(source.get("heading", typography.heading));
    typography.stat = VariantTools::as_int(source.get("stat", typography.stat));
    typography.subheading = VariantTools::as_int(source.get("subheading", typography.subheading));
    typography.body = VariantTools::as_int(source.get("body", typography.body));
    typography.caption = VariantTools::as_int(source.get("caption", typography.caption));
    typography.card_body = VariantTools::as_int(source.get("card_body", typography.card_body));
    typography.micro = VariantTools::as_int(source.get("micro", typography.micro));
    return typography;
}

UiSpacing parse_spacing(const Dictionary &source, UiSpacing spacing) {
    spacing.xs = VariantTools::as_int(source.get("xs", spacing.xs));
    spacing.sm = VariantTools::as_int(source.get("sm", spacing.sm));
    spacing.md = VariantTools::as_int(source.get("md", spacing.md));
    spacing.lg = VariantTools::as_int(source.get("lg", spacing.lg));
    spacing.xl = VariantTools::as_int(source.get("xl", spacing.xl));
    spacing.screen_margin = VariantTools::as_int(source.get("screen_margin", spacing.screen_margin));
    spacing.section_gap = VariantTools::as_int(source.get("section_gap", spacing.section_gap));
    return spacing;
}

UiShape parse_shape(const Dictionary &source, UiShape shape) {
    shape.corner_sm = VariantTools::as_int(source.get("corner_sm", shape.corner_sm));
    shape.corner_md = VariantTools::as_int(source.get("corner_md", shape.corner_md));
    shape.corner_lg = VariantTools::as_int(source.get("corner_lg", shape.corner_lg));
    shape.corner_pill = VariantTools::as_int(source.get("corner_pill", shape.corner_pill));
    shape.border_width = VariantTools::as_int(source.get("border_width", shape.border_width));
    shape.border_width_strong = VariantTools::as_int(source.get("border_width_strong", shape.border_width_strong));
    return shape;
}

UiSurfaceStyle parse_surface(const Dictionary &source) {
    UiSurfaceStyle surface;
    surface.bg_role = parse_role(source, "bg", surface.bg_role);
    surface.border_role = parse_role(source, "border", surface.border_role);
    surface.shape_role = parse_role(source, "shape", surface.shape_role);
    surface.border_width_role = parse_role(source, "border_width", surface.border_width_role);
    surface.content_margin_role = parse_role(source, "content_margin", surface.content_margin_role);
    surface.shadow_size = VariantTools::as_int(source.get("shadow_size", surface.shadow_size));
    surface.shadow_role = parse_role(source, "shadow", surface.shadow_role);
    return surface;
}

UiButtonState parse_button_state(const Dictionary &source, const char *key, const UiButtonState &fallback) {
    const Dictionary state_dict = source.get(key, Dictionary());
    UiButtonState state = fallback;
    state.bg_role = parse_role(state_dict, "bg", state.bg_role);
    state.border_role = parse_role(state_dict, "border", state.border_role);
    state.font_role = parse_role(state_dict, "font", state.font_role);
    return state;
}

UiButtonVariant parse_button(const Dictionary &source) {
    UiButtonVariant button;
    const Array min_size = source.get("min_size", Array());
    if (min_size.size() >= 2) {
        button.min_width = VariantTools::as_int(min_size[0]);
        button.min_height = VariantTools::as_int(min_size[1]);
    }
    button.font_size_role = parse_role(source, "font_size", button.font_size_role);
    button.shape_role = parse_role(source, "shape", button.shape_role);
    button.content_margin_role = parse_role(source, "content_margin", button.content_margin_role);
    button.normal = parse_button_state(source, "normal", {.bg_role = "surface", .border_role = "border", .font_role = "text_primary"});
    button.hover = parse_button_state(source, "hover", button.normal);
    button.pressed = parse_button_state(source, "pressed", button.normal);
    button.disabled = parse_button_state(source, "disabled", button.normal);
    button.focus = parse_button_state(source, "focus", button.normal);
    return button;
}

UiTextStyle parse_text_style(const Dictionary &source) {
    UiTextStyle text_style;
    text_style.font_size_role = parse_role(source, "font_size", text_style.font_size_role);
    text_style.color_role = parse_role(source, "color", text_style.color_role);
    text_style.outline_size = VariantTools::as_int(source.get("outline_size", text_style.outline_size));
    text_style.outline_role = parse_role(source, "outline", text_style.outline_role);
    return text_style;
}

UiMedallionStyle parse_medallion(const Dictionary &source) {
    UiMedallionStyle medallion;
    medallion.mark = parse_role(source, "mark", medallion.mark);
    medallion.color_role = parse_role(source, "color", medallion.color_role);
    return medallion;
}

UiScreenStyle parse_screen(const Dictionary &source, UiScreenStyle screen) {
    screen.backdrop_role = parse_role(source, "backdrop", screen.backdrop_role);
    screen.title_text_style = parse_role(source, "title_text_style", screen.title_text_style);
    screen.subtitle_text_style = parse_role(source, "subtitle_text_style", screen.subtitle_text_style);
    screen.panel_surface = parse_role(source, "panel_surface", screen.panel_surface);
    screen.content_max_width_ratio = VariantTools::as_float(source.get("content_max_width_ratio", screen.content_max_width_ratio));
    screen.content_max_height_ratio = VariantTools::as_float(source.get("content_max_height_ratio", screen.content_max_height_ratio));
    screen.footer_gap_role = parse_role(source, "footer_gap", screen.footer_gap_role);
    return screen;
}

UiSoundData parse_sound(const Dictionary &source) {
    UiSoundData sound;
    sound.path = to_std_string(String(source.get("path", "")));
    sound.volume_linear = VariantTools::as_float(source.get("volume_linear", sound.volume_linear));
    return sound;
}

UiSfxData parse_sfx(const Dictionary &source) {
    return {
        .hover = parse_sound(source.get("hover", Dictionary())),
        .click = parse_sound(source.get("click", Dictionary())),
        .deploy_card = parse_sound(source.get("deploy_card", Dictionary())),
    };
}

template <typename Parser, typename Map> void parse_named_entries(const Dictionary &source, Map &target, Parser parser) {
    const Array names = source.keys();
    for (const Variant &name_variant : names) {
        const String name = name_variant;
        target.emplace(to_std_string(name), parser(Dictionary(source[name])));
    }
}

} // namespace

std::optional<UiThemeData> UiThemeLoader::load(const String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "UiThemeLoader");
    return data ? std::optional<UiThemeData>(load_from_data(*data)) : std::nullopt;
}

UiThemeData UiThemeLoader::load_from_data(const Dictionary &data) {
    UiThemeData theme;
    theme.font_path = to_std_string(String(data.get("font", "")));
    theme.palette = parse_palette(data.get("palette", Dictionary()), theme.palette);
    theme.typography = parse_typography(data.get("typography", Dictionary()), theme.typography);
    theme.spacing = parse_spacing(data.get("spacing", Dictionary()), theme.spacing);
    theme.shape = parse_shape(data.get("shape", Dictionary()), theme.shape);
    parse_named_entries(data.get("surfaces", Dictionary()), theme.surfaces, parse_surface);
    parse_named_entries(data.get("buttons", Dictionary()), theme.buttons, parse_button);
    parse_named_entries(data.get("text_styles", Dictionary()), theme.text_styles, parse_text_style);
    parse_named_entries(data.get("medallions", Dictionary()), theme.medallions, parse_medallion);
    const Dictionary metrics = data.get("metrics", Dictionary());
    for (const Variant &name_variant : Array(metrics.keys())) {
        const String name = name_variant;
        theme.metrics.emplace(to_std_string(name), VariantTools::as_int(metrics[name]));
    }
    theme.screen = parse_screen(data.get("screen", Dictionary()), theme.screen);
    theme.sfx = parse_sfx(data.get("sfx", Dictionary()));
    return theme;
}

} // namespace defn
