// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_theme_provider.h"

#include "data_paths.h"
#include "godot_color.h"
#include "godot_string.h"
#include "ui_theme_loader.h"

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <array>
#include <cctype>
#include <optional>
#include <string>

namespace defn {

using namespace godot;

namespace {

std::optional<UiThemeData> g_data;
Ref<Theme> g_theme;

std::string to_pascal_case(std::string_view snake_case) {
    std::string pascal;
    pascal.reserve(snake_case.size());
    bool capitalize = true;
    for (const char character : snake_case) {
        if (character == '_') {
            capitalize = true;
            continue;
        }
        pascal.push_back(capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(character))) : character);
        capitalize = false;
    }
    return pascal;
}

// The "Defn" prefix keeps generated variation names from colliding with built-in Godot control classes (for example MenuButton).
StringName variation_name(std::string_view entry_name, const char *suffix) { return to_godot_string("Defn" + to_pascal_case(entry_name) + suffix); }

godot::Color role_color(const UiThemeData &theme_data, std::string_view role) {
    const auto found = theme_data.find_color_role(role);
    return found.has_value() ? to_godot_color(*found) : godot::Color(1, 1, 1, 1);
}

int role_shape(const UiThemeData &theme_data, std::string_view role, int fallback) {
    const auto found = theme_data.find_shape_role(role);
    return found.value_or(fallback);
}

int role_spacing(const UiThemeData &theme_data, std::string_view role, int fallback) {
    const auto found = theme_data.find_spacing_role(role);
    return found.value_or(fallback);
}

int role_font_size(const UiThemeData &theme_data, std::string_view role, int fallback) {
    const auto found = theme_data.find_font_size_role(role);
    return found.value_or(fallback);
}

Ref<StyleBoxFlat> build_surface_style(const UiThemeData &theme_data, const UiSurfaceStyle &surface) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(role_color(theme_data, surface.bg_role));
    style->set_border_color(role_color(theme_data, surface.border_role));
    style->set_border_width_all(role_shape(theme_data, surface.border_width_role, theme_data.shape.border_width));
    style->set_corner_radius_all(role_shape(theme_data, surface.shape_role, theme_data.shape.corner_md));
    if (!surface.content_margin_role.empty()) {
        style->set_content_margin_all(static_cast<float>(role_spacing(theme_data, surface.content_margin_role, theme_data.spacing.md)));
    }
    if (surface.shadow_size > 0) {
        godot::Color shadow = role_color(theme_data, surface.shadow_role);
        shadow.a = 0.72F;
        style->set_shadow_color(shadow);
        style->set_shadow_size(surface.shadow_size);
    }
    return style;
}

Ref<StyleBoxFlat> build_button_state_style(const UiThemeData &theme_data, const UiButtonVariant &button, const UiButtonState &state) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(role_color(theme_data, state.bg_role));
    style->set_border_color(role_color(theme_data, state.border_role));
    style->set_border_width_all(role_shape(theme_data, button.border_width_role, theme_data.shape.border_width));
    style->set_corner_radius_all(role_shape(theme_data, button.shape_role, theme_data.shape.corner_md));
    if (!button.content_margin_role.empty()) {
        style->set_content_margin_all(static_cast<float>(role_spacing(theme_data, button.content_margin_role, theme_data.spacing.md)));
    }
    return style;
}

void register_button_type(const Ref<Theme> &theme, const UiThemeData &theme_data, const StringName &type_name, const UiButtonVariant &button) {
    theme->set_stylebox("normal", type_name, build_button_state_style(theme_data, button, button.normal));
    theme->set_stylebox("hover", type_name, build_button_state_style(theme_data, button, button.hover));
    theme->set_stylebox("pressed", type_name, build_button_state_style(theme_data, button, button.pressed));
    theme->set_stylebox("disabled", type_name, build_button_state_style(theme_data, button, button.disabled));
    theme->set_stylebox("focus", type_name, build_button_state_style(theme_data, button, button.focus));
    theme->set_color("font_color", type_name, role_color(theme_data, button.normal.font_role));
    theme->set_color("font_hover_color", type_name, role_color(theme_data, button.hover.font_role));
    theme->set_color("font_pressed_color", type_name, role_color(theme_data, button.pressed.font_role));
    theme->set_color("font_disabled_color", type_name, role_color(theme_data, button.disabled.font_role));
    theme->set_color("font_focus_color", type_name, role_color(theme_data, button.focus.font_role));
    theme->set_font_size("font_size", type_name, role_font_size(theme_data, button.font_size_role, theme_data.typography.body));
}

void register_label_type(const Ref<Theme> &theme, const UiThemeData &theme_data, const StringName &type_name, const UiTextStyle &text_style) {
    theme->set_color("font_color", type_name, role_color(theme_data, text_style.color_role));
    theme->set_font_size("font_size", type_name, role_font_size(theme_data, text_style.font_size_role, theme_data.typography.body));
    theme->set_constant("outline_size", type_name, text_style.outline_size);
    if (text_style.outline_size > 0) {
        theme->set_color("font_outline_color", type_name, role_color(theme_data, text_style.outline_role));
    }
}

/// A white mark recoloured into a texture at the size the theme asks for. The medallion marks the views build
/// are tinted live through `modulate`, but Godot draws most of a control's own icons unmodulated, so those have
/// to arrive already in the palette.
Ref<Texture2D> build_control_icon(const UiThemeData &theme_data, std::string_view name) {
    const UiMedallionStyle *style = theme_data.find_control_icon(name);
    if (style == nullptr || style->mark.empty()) {
        return {};
    }

    const Ref<Texture2D> source = ResourceLoader::get_singleton()->load(to_godot_string(style->mark));
    if (source.is_null()) {
        UtilityFunctions::printerr("UiThemeProvider: Failed to load control icon ", to_godot_string(style->mark));
        return {};
    }

    Ref<Image> image = source->get_image();
    if (image.is_null()) {
        return {};
    }
    image = image->duplicate();
    image->convert(Image::FORMAT_RGBA8);
    if (const int size = theme_data.metric("control_icon_size", 0); size > 0) {
        image->resize(size, size, Image::INTERPOLATE_LANCZOS);
    }

    // The marks are solid white with a shaped alpha channel, so recolouring is a straight channel swap that
    // leaves every edge exactly as the SVG anti-aliased it.
    const godot::Color tint = role_color(theme_data, style->color_role);
    for (int row = 0; row < image->get_height(); ++row) {
        for (int column = 0; column < image->get_width(); ++column) {
            const godot::Color pixel = image->get_pixel(column, row);
            image->set_pixel(column, row, godot::Color(tint.r, tint.g, tint.b, pixel.a * tint.a));
        }
    }
    return ImageTexture::create_from_image(image);
}

/// Replaces the marks Godot's own controls draw for themselves. Without these a slider knob, a dropdown arrow
/// and a popup's selection dot render in the engine's stock greys, which is the one place the palette cannot
/// reach through styleboxes alone.
void register_control_icons(const Ref<Theme> &theme, const UiThemeData &theme_data) {
    const Ref<Texture2D> knob = build_control_icon(theme_data, "slider_knob");
    const Ref<Texture2D> knob_hot = build_control_icon(theme_data, "slider_knob_hot");
    const Ref<Texture2D> knob_off = build_control_icon(theme_data, "slider_knob_disabled");
    for (const char *slider : {"HSlider", "VSlider"}) {
        theme->set_icon("grabber", slider, knob);
        theme->set_icon("grabber_highlight", slider, knob_hot);
        theme->set_icon("grabber_disabled", slider, knob_off);
    }

    const Ref<Texture2D> chosen = build_control_icon(theme_data, "choice_on");
    const Ref<Texture2D> unchosen = build_control_icon(theme_data, "choice_off");
    theme->set_icon("radio_checked", "PopupMenu", chosen);
    theme->set_icon("radio_unchecked", "PopupMenu", unchosen);
    theme->set_icon("checked", "PopupMenu", chosen);
    theme->set_icon("unchecked", "PopupMenu", unchosen);

    // Godot only tints the dropdown arrow with the button's font colours when asked to; left off it stays the
    // engine's own grey next to text that is not.
    theme->set_constant("modulate_arrow", "OptionButton", 1);
}

void register_base_types(const Ref<Theme> &theme, const UiThemeData &theme_data) {
    const godot::Color text_primary = to_godot_color(theme_data.palette.text_primary);
    const godot::Color text_secondary = to_godot_color(theme_data.palette.text_secondary);
    const godot::Color text_muted = to_godot_color(theme_data.palette.text_muted);
    const godot::Color accent = to_godot_color(theme_data.palette.accent);
    const godot::Color accent_strong = to_godot_color(theme_data.palette.accent_strong);

    theme->set_color("font_color", "Label", text_primary);
    theme->set_font_size("font_size", "Label", theme_data.typography.body);

    if (const UiSurfaceStyle *panel = theme_data.find_surface("panel"); panel != nullptr) {
        theme->set_stylebox("panel", "PanelContainer", build_surface_style(theme_data, *panel));
        theme->set_stylebox("panel", "Panel", build_surface_style(theme_data, *panel));
        theme->set_stylebox("panel", "PopupMenu", build_surface_style(theme_data, *panel));
    }

    if (const UiButtonVariant *menu_button = theme_data.find_button("menu"); menu_button != nullptr) {
        register_button_type(theme, theme_data, "Button", *menu_button);
        register_button_type(theme, theme_data, "OptionButton", *menu_button);
        register_button_type(theme, theme_data, "CheckButton", *menu_button);
    }

    theme->set_color("font_color", "PopupMenu", text_primary);
    theme->set_color("font_hover_color", "PopupMenu", accent);
    theme->set_color("font_disabled_color", "PopupMenu", text_muted);
    theme->set_font_size("font_size", "PopupMenu", theme_data.typography.body);
    if (const UiSurfaceStyle *card = theme_data.find_surface("card"); card != nullptr) {
        theme->set_stylebox("hover", "PopupMenu", build_surface_style(theme_data, *card));
    }

    if (const UiSurfaceStyle *backplate = theme_data.find_surface("backplate"); backplate != nullptr) {
        const Ref<StyleBoxFlat> track = build_surface_style(theme_data, *backplate);
        theme->set_stylebox("slider", "HSlider", track);
        theme->set_stylebox("slider", "VSlider", track);
        theme->set_stylebox("scroll", "HScrollBar", track);
        theme->set_stylebox("scroll", "VScrollBar", track);
    }

    Ref<StyleBoxFlat> grabber;
    grabber.instantiate();
    grabber->set_bg_color(accent);
    grabber->set_corner_radius_all(theme_data.shape.corner_sm);
    Ref<StyleBoxFlat> grabber_highlight;
    grabber_highlight.instantiate();
    grabber_highlight->set_bg_color(accent_strong);
    grabber_highlight->set_corner_radius_all(theme_data.shape.corner_sm);
    theme->set_stylebox("grabber_area", "HSlider", grabber);
    theme->set_stylebox("grabber_area_highlight", "HSlider", grabber_highlight);
    theme->set_stylebox("grabber", "HScrollBar", grabber);
    theme->set_stylebox("grabber", "VScrollBar", grabber);
    theme->set_stylebox("grabber_highlight", "HScrollBar", grabber_highlight);
    theme->set_stylebox("grabber_highlight", "VScrollBar", grabber_highlight);
    theme->set_stylebox("grabber_pressed", "HScrollBar", grabber_highlight);
    theme->set_stylebox("grabber_pressed", "VScrollBar", grabber_highlight);

    theme->set_constant("separation", "VBoxContainer", theme_data.spacing.md);
    theme->set_constant("separation", "HBoxContainer", theme_data.spacing.md);
    theme->set_color("font_unselected_color", "TabContainer", text_secondary);
}

Ref<Theme> build_theme(const UiThemeData &theme_data) {
    Ref<Theme> theme;
    theme.instantiate();

    if (!theme_data.font_path.empty()) {
        const Ref<Font> font = ResourceLoader::get_singleton()->load(to_godot_string(theme_data.font_path));
        if (font.is_valid()) {
            theme->set_default_font(font);
        } else {
            UtilityFunctions::printerr("UiThemeProvider: Failed to load font ", to_godot_string(theme_data.font_path));
        }
    }
    theme->set_default_font_size(theme_data.typography.body);

    register_base_types(theme, theme_data);
    register_control_icons(theme, theme_data);

    for (const auto &[name, text_style] : theme_data.text_styles) {
        const StringName type_name = variation_name(name, "Label");
        theme->set_type_variation(type_name, "Label");
        register_label_type(theme, theme_data, type_name, text_style);
    }

    for (const auto &[name, button] : theme_data.buttons) {
        const StringName type_name = variation_name(name, "Button");
        theme->set_type_variation(type_name, "Button");
        register_button_type(theme, theme_data, type_name, button);
    }

    for (const auto &[name, surface] : theme_data.surfaces) {
        const StringName type_name = variation_name(name, "Panel");
        theme->set_type_variation(type_name, "PanelContainer");
        theme->set_stylebox("panel", type_name, build_surface_style(theme_data, surface));
    }

    return theme;
}

} // namespace

const UiThemeData &UiThemeProvider::data() {
    if (!g_data.has_value()) {
        auto loaded = UiThemeLoader::load(DataPaths::UI_THEME);
        if (!loaded.has_value()) {
            UtilityFunctions::printerr("UiThemeProvider: Failed to load ", String(DataPaths::UI_THEME), "; using built-in defaults");
            loaded = UiThemeData{};
        }
        g_data = std::move(*loaded);
    }
    return *g_data;
}

Ref<Theme> UiThemeProvider::theme() {
    if (g_theme.is_null()) {
        g_theme = build_theme(data());
    }
    return g_theme;
}

void UiThemeProvider::install(SceneTree *tree) {
    if (tree != nullptr && tree->get_root() != nullptr) {
        tree->get_root()->set_theme(theme());
    }
}

void UiThemeProvider::apply_to(Control *control) {
    if (control != nullptr) {
        control->set_theme(theme());
    }
}

void UiThemeProvider::reload() {
    g_data.reset();
    g_theme.unref();
}

godot::Color UiThemeProvider::color(std::string_view role) { return role_color(data(), role); }

real_t UiThemeProvider::metric(std::string_view name, int fallback) { return static_cast<real_t>(data().metric(name, fallback)); }

int UiThemeProvider::font_size(std::string_view role) { return role_font_size(data(), role, data().typography.body); }

int UiThemeProvider::spacing(std::string_view role) { return role_spacing(data(), role, data().spacing.md); }

int UiThemeProvider::shape(std::string_view role) { return role_shape(data(), role, data().shape.corner_md); }

double UiThemeProvider::motion(std::string_view role) { return data().find_motion_role(role).value_or(data().motion.base); }

Ref<StyleBoxFlat> UiThemeProvider::surface(std::string_view name) {
    const UiThemeData &theme_data = data();
    const UiSurfaceStyle *style = theme_data.find_surface(name);
    static const UiSurfaceStyle fallback;
    return build_surface_style(theme_data, style != nullptr ? *style : fallback);
}

StringName UiThemeProvider::label_variation(std::string_view text_style_name) { return variation_name(text_style_name, "Label"); }

StringName UiThemeProvider::button_variation(std::string_view button_name) { return variation_name(button_name, "Button"); }

StringName UiThemeProvider::panel_variation(std::string_view surface_name) { return variation_name(surface_name, "Panel"); }

} // namespace defn
