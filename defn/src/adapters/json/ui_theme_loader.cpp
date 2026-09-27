// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_theme_loader.h"

#include "godot_string.h"
#include "json_file_loader.h"
#include "variant_tools.h"

#include <godot_cpp/variant/array.hpp>

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace defn {

using namespace godot;

namespace {

constexpr float DEFAULT_ALPHA = 1.0F;

/// How many references one palette entry may follow before the chain is treated as broken. Chains in practice
/// are one hop deep; the limit only exists so a cycle cannot hang the load.
constexpr int MAX_PALETTE_REFERENCE_DEPTH = 8;

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

/// One palette entry exactly as written, before references are resolved. A literal entry carries its own
/// colour; a reference entry names the role it borrows from, optionally at a different opacity.
struct PaletteEntry {
    Color literal;
    std::string reference;
    std::optional<float> alpha_override;
};

/// A palette entry is either a literal `[r, g, b, a]`, a reference `"other_role"`, or a reference at a new
/// opacity `["other_role", alpha]`. References are what let the semantic names sit on top of one ramp
/// instead of restating its values, so a change to the ramp reaches every name that borrows from it.
PaletteEntry parse_palette_entry(const Variant &value) {
    PaletteEntry entry;
    if (value.get_type() == Variant::STRING) {
        entry.reference = to_std_string(String(value));
        return entry;
    }

    const Array values = value;
    if (!values.is_empty() && values[0].get_type() == Variant::STRING) {
        entry.reference = to_std_string(String(values[0]));
        if (values.size() >= 2) {
            entry.alpha_override = VariantTools::as_float(values[1]);
        }
        return entry;
    }

    entry.literal = parse_color(values, Color{});
    return entry;
}

/// Follows every reference chain down to the literal it ends at. The nearest opacity override on the way
/// wins, so `["neutral_ink", 0.72]` reads as "the ink colour, at 72% opacity".
///
/// A role whose chain is broken or circular is left out of the result rather than resolved to black:
/// `ContentValidator` then reports each place that names it, which is where the mistake actually is.
std::map<std::string, Color, std::less<>> resolve_palette(const Dictionary &source) {
    std::map<std::string, PaletteEntry, std::less<>> entries;
    for (const Variant &raw_key : Array(source.keys())) {
        const String key = raw_key;
        entries.insert_or_assign(to_std_string(key), parse_palette_entry(source[key]));
    }

    std::map<std::string, Color, std::less<>> resolved;
    for (const auto &[name, entry] : entries) {
        const PaletteEntry *current = &entry;
        std::optional<float> alpha;
        for (int depth = 0; current != nullptr && !current->reference.empty() && depth < MAX_PALETTE_REFERENCE_DEPTH; ++depth) {
            if (!alpha.has_value()) {
                alpha = current->alpha_override;
            }
            const auto target = entries.find(current->reference);
            current = target == entries.end() ? nullptr : &target->second;
        }

        if (current == nullptr || !current->reference.empty()) {
            continue;
        }

        Color color = current->literal;
        if (alpha.has_value()) {
            color.a = *alpha;
        }
        resolved.insert_or_assign(name, color);
    }
    return resolved;
}

UiPalette parse_palette(const Dictionary &source, UiPalette palette) {
    std::map<std::string, Color, std::less<>> resolved = resolve_palette(source);

    const auto assign = [&resolved](Color &target, std::string_view role) {
        if (const auto found = resolved.find(role); found != resolved.end()) {
            target = found->second;
        }
    };

    assign(palette.surface, "surface");
    assign(palette.surface_raised, "surface_raised");
    assign(palette.surface_sunken, "surface_sunken");
    assign(palette.overlay_scrim, "overlay_scrim");
    assign(palette.border, "border");
    assign(palette.border_strong, "border_strong");
    assign(palette.border_focus, "border_focus");
    assign(palette.accent, "accent");
    assign(palette.accent_strong, "accent_strong");
    assign(palette.text_primary, "text_primary");
    assign(palette.text_secondary, "text_secondary");
    assign(palette.text_muted, "text_muted");
    assign(palette.text_inverse, "text_inverse");
    assign(palette.state_success, "state_success");
    assign(palette.state_warning, "state_warning");
    assign(palette.state_danger, "state_danger");
    assign(palette.state_locked, "state_locked");
    assign(palette.energy, "energy");
    assign(palette.victory, "victory");
    assign(palette.defeat, "defeat");
    assign(palette.overlay_victory, "overlay_victory");
    assign(palette.overlay_defeat, "overlay_defeat");
    assign(palette.backdrop, "backdrop");
    assign(palette.scrim_soft, "scrim_soft");
    assign(palette.scrim_panel, "scrim_panel");
    assign(palette.route_locked, "route_locked");
    assign(palette.locked_tint, "locked_tint");
    assign(palette.ambience_dust, "ambience_dust");
    assign(palette.ambience_spores, "ambience_spores");
    assign(palette.ambience_mist, "ambience_mist");
    assign(palette.ambience_snow, "ambience_snow");
    assign(palette.ambience_embers, "ambience_embers");
    assign(palette.transparent, "transparent");

    palette.extra = std::move(resolved);
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

UiMotion parse_motion(const Dictionary &source, UiMotion motion) {
    motion.fast = VariantTools::as_float(source.get("fast", motion.fast));
    motion.base = VariantTools::as_float(source.get("base", motion.base));
    motion.slow = VariantTools::as_float(source.get("slow", motion.slow));
    return motion;
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

std::optional<UiSelectionStyle> parse_selection(const Dictionary &source) {
    if (!source.has("selected")) {
        return std::nullopt;
    }
    const Dictionary selected = source.get("selected", Dictionary());
    UiSelectionStyle selection;
    selection.bg_role = parse_role(selected, "bg", selection.bg_role);
    selection.hover_bg_role = parse_role(selected, "hover", selection.hover_bg_role);
    selection.border_role = parse_role(selected, "border", selection.border_role);
    selection.border_width_role = parse_role(selected, "border_width", selection.border_width_role);
    return selection;
}

UiButtonVariant parse_button(const Dictionary &source) {
    UiButtonVariant button;
    const Array min_size = source.get("min_size", Array());
    if (min_size.size() >= 2) {
        button.min_width = VariantTools::as_int(min_size[0]);
        button.min_height = VariantTools::as_int(min_size[1]);
    }
    button.sfx_role = parse_role(source, "sfx", button.sfx_role);
    button.font_size_role = parse_role(source, "font_size", button.font_size_role);
    button.shape_role = parse_role(source, "shape", button.shape_role);
    button.border_width_role = parse_role(source, "border_width", button.border_width_role);
    button.content_margin_role = parse_role(source, "content_margin", button.content_margin_role);
    button.selected = parse_selection(source);
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

/// A variant that declares a `selected` block gains a derived `<name>_selected` sibling. Deriving it here
/// leaves one representation downstream: the provider, `find_button` and `ContentValidator` all see an
/// ordinary variant rather than each having to know that selection is a thing.
void derive_selected_variants(std::map<std::string, UiButtonVariant, std::less<>> &buttons) {
    std::vector<std::pair<std::string, UiButtonVariant>> derived;
    for (const auto &[name, variant] : buttons) {
        if (variant.selected.has_value()) {
            derived.emplace_back(name + "_selected", apply_selection(variant, *variant.selected));
        }
    }
    for (auto &[name, variant] : derived) {
        buttons.insert_or_assign(name, std::move(variant));
    }
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
    theme.motion = parse_motion(data.get("motion", Dictionary()), theme.motion);
    parse_named_entries(data.get("surfaces", Dictionary()), theme.surfaces, parse_surface);
    parse_named_entries(data.get("buttons", Dictionary()), theme.buttons, parse_button);
    derive_selected_variants(theme.buttons);
    parse_named_entries(data.get("text_styles", Dictionary()), theme.text_styles, parse_text_style);
    parse_named_entries(data.get("medallions", Dictionary()), theme.medallions, parse_medallion);
    parse_named_entries(data.get("icons", Dictionary()), theme.icons, parse_medallion);
    parse_named_entries(data.get("control_icons", Dictionary()), theme.control_icons, parse_medallion);
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
