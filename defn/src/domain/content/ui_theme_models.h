// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_THEME_MODELS_H
#define UI_THEME_MODELS_H

#include "content_values.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace defn {

struct UiSoundData {
    std::string path;
    float volume_linear = 0.2F;
};

struct UiSfxData {
    UiSoundData hover;
    UiSoundData click;
    UiSoundData deploy_card;

    [[nodiscard]] const UiSoundData &press(std::string_view role) const { return role == "deploy_card" ? deploy_card : click; }
};

/// The ramp `data/ui_theme.json` builds its palette from, restated so a theme that fails to load still renders
/// in the designed colours instead of an unrelated set. The semantic roles below borrow from these exactly as
/// the JSON roles reference them, so the two stay the same design; keep them in step.
namespace palette_ramp {

inline constexpr Color INK = {0.021F, 0.027F, 0.049F, 1.0F};
inline constexpr Color SUNKEN = {0.043F, 0.052F, 0.087F, 1.0F};
inline constexpr Color SURFACE = {0.073F, 0.086F, 0.137F, 1.0F};
inline constexpr Color RAISED = {0.112F, 0.129F, 0.198F, 1.0F};
inline constexpr Color LINE = {0.328F, 0.365F, 0.512F, 1.0F};
inline constexpr Color LINE_STRONG = {0.504F, 0.542F, 0.696F, 1.0F};
inline constexpr Color ACCENT = {0.982F, 0.811F, 0.298F, 1.0F};
inline constexpr Color ACCENT_STRONG = {0.981F, 0.870F, 0.539F, 1.0F};
inline constexpr Color TEXT_PRIMARY = {0.948F, 0.953F, 0.972F, 1.0F};
inline constexpr Color TEXT_SECONDARY = {0.791F, 0.803F, 0.849F, 1.0F};
inline constexpr Color TEXT_MUTED = {0.590F, 0.610F, 0.690F, 1.0F};
inline constexpr Color SUCCESS = {0.340F, 0.780F, 0.575F, 1.0F};
inline constexpr Color WARNING = {0.940F, 0.713F, 0.260F, 1.0F};
inline constexpr Color DANGER = {0.888F, 0.312F, 0.331F, 1.0F};
inline constexpr Color LOCKED = {0.370F, 0.400F, 0.470F, 1.0F};

/// A ramp entry at a different opacity, for the roles that differ from their source only in transparency.
constexpr Color at_alpha(Color color, float alpha) {
    color.a = alpha;
    return color;
}

} // namespace palette_ramp

struct UiPalette {
    Color surface = palette_ramp::SURFACE;
    Color surface_raised = palette_ramp::RAISED;
    Color surface_sunken = palette_ramp::SUNKEN;
    Color overlay_scrim = palette_ramp::at_alpha(palette_ramp::INK, 0.72F);
    Color border = palette_ramp::LINE;
    Color border_strong = palette_ramp::LINE_STRONG;
    Color border_focus = palette_ramp::ACCENT;
    Color accent = palette_ramp::ACCENT;
    Color accent_strong = palette_ramp::ACCENT_STRONG;
    Color text_primary = palette_ramp::TEXT_PRIMARY;
    Color text_secondary = palette_ramp::TEXT_SECONDARY;
    Color text_muted = palette_ramp::TEXT_MUTED;
    Color text_inverse = palette_ramp::INK;
    Color state_success = palette_ramp::SUCCESS;
    Color state_warning = palette_ramp::WARNING;
    Color state_danger = palette_ramp::DANGER;
    Color state_locked = palette_ramp::LOCKED;
    Color energy = {0.376F, 0.702F, 0.949F, 1.0F};
    Color victory = palette_ramp::SUCCESS;
    Color defeat = palette_ramp::DANGER;
    Color overlay_victory = {0.055F, 0.145F, 0.108F, 0.42F};
    Color overlay_defeat = {0.160F, 0.045F, 0.048F, 0.46F};
    Color backdrop = palette_ramp::SUNKEN;
    Color scrim_soft = palette_ramp::at_alpha(palette_ramp::INK, 0.18F);
    Color scrim_panel = palette_ramp::at_alpha(palette_ramp::SUNKEN, 0.82F);
    Color route_locked = palette_ramp::at_alpha(palette_ramp::LOCKED, 0.48F);
    Color locked_tint = {0.560F, 0.580F, 0.640F, 0.45F};
    Color ambience_dust = {0.820F, 0.650F, 0.360F, 0.42F};
    Color ambience_spores = {0.480F, 0.780F, 0.500F, 0.40F};
    Color ambience_mist = {0.550F, 0.820F, 0.860F, 0.30F};
    Color ambience_snow = {0.820F, 0.910F, 1.000F, 0.52F};
    Color ambience_embers = {0.950F, 0.400F, 0.220F, 0.48F};
    Color transparent = {0.0F, 0.0F, 0.0F, 0.0F};
    std::map<std::string, Color, std::less<>> extra;
};

struct UiTypography {
    int banner = 72;
    int display = 48;
    int title = 34;
    int menu = 32;
    int section = 28;
    int heading = 24;
    int stat = 22;
    int subheading = 20;
    int body = 18;
    int caption = 15;
    int card_body = 14;
    int micro = 13;
};

struct UiSpacing {
    int xs = 4;
    int sm = 8;
    int md = 12;
    int lg = 18;
    int xl = 32;
    int screen_margin = 32;
    int section_gap = 16;
};

/// How long a transition runs, in seconds. Three steps are enough for a UI this size, and naming them is what
/// stops every fade and lift in the game picking a duration of its own.
struct UiMotion {
    float fast = 0.12F;
    float base = 0.18F;
    float slow = 0.26F;
};

struct UiShape {
    int corner_sm = 4;
    int corner_md = 8;
    int corner_lg = 12;
    int corner_pill = 19;
    int border_width = 2;
    int border_width_strong = 4;
};

struct UiSurfaceStyle {
    std::string bg_role = "surface";
    std::string border_role = "border";
    std::string shape_role = "corner_md";
    std::string border_width_role = "border_width";
    std::string content_margin_role;
    int shadow_size = 0;
    std::string shadow_role;
};

struct UiButtonState {
    std::string bg_role;
    std::string border_role;
    std::string font_role;
};

/// How a variant shows that its card is the chosen one. Expressing selection once, as a change to the frame,
/// is what stops the roster, the upgrade picker and the campaign map drifting into three different ideas of it.
struct UiSelectionStyle {
    std::string bg_role;
    std::string hover_bg_role;
    std::string border_role = "accent";
    std::string border_width_role = "border_width_strong";
};

struct UiButtonVariant {
    int min_width = 0;
    /// Which entry in `sfx` this variant plays when pressed. Naming it here is what stops a call site having to
    /// remember which of the game's sounds a given control is supposed to make.
    std::string sfx_role = "click";
    int min_height = 0;
    std::string font_size_role = "body";
    std::string shape_role = "corner_md";
    std::string border_width_role = "border_width";
    std::string content_margin_role;
    UiButtonState normal;
    UiButtonState hover;
    UiButtonState pressed;
    UiButtonState disabled;
    UiButtonState focus;
    std::optional<UiSelectionStyle> selected;
};

/// The same variant with its frame swapped for the selected treatment: identical footprint, identical type,
/// one unmistakable change. The hover background stays distinct so a selected card still answers the cursor.
[[nodiscard]] UiButtonVariant apply_selection(UiButtonVariant variant, const UiSelectionStyle &selection);

struct UiTextStyle {
    std::string font_size_role = "body";
    std::string color_role = "text_primary";
    int outline_size = 0;
    std::string outline_role;
};

/// A tinted vector mark: the SVG texture and the palette role it is modulated with. Shared by the campaign map
/// node states, the HUD instrument icons, and the marks Godot's built-in controls draw for themselves.
struct UiMedallionStyle {
    std::string mark;
    std::string color_role = "text_primary";
};

struct UiScreenStyle {
    std::string backdrop_role = "overlay_scrim";
    std::string title_text_style = "screen_title";
    std::string subtitle_text_style = "secondary";
    std::string panel_surface = "panel";
    float content_max_width_ratio = 0.8F;
    float content_max_height_ratio = 0.85F;
    std::string footer_gap_role = "md";
};

struct UiThemeData {
    UiPalette palette;
    UiTypography typography;
    UiSpacing spacing;
    UiShape shape;
    UiMotion motion;
    std::string font_path;
    std::map<std::string, UiSurfaceStyle, std::less<>> surfaces;
    std::map<std::string, UiButtonVariant, std::less<>> buttons;
    std::map<std::string, UiTextStyle, std::less<>> text_styles;
    std::map<std::string, UiMedallionStyle, std::less<>> medallions;
    std::map<std::string, UiMedallionStyle, std::less<>> icons;
    std::map<std::string, UiMedallionStyle, std::less<>> control_icons;
    std::map<std::string, int, std::less<>> metrics;
    UiScreenStyle screen;
    UiSfxData sfx;

    [[nodiscard]] const UiSurfaceStyle *find_surface(std::string_view name) const;
    [[nodiscard]] const UiButtonVariant *find_button(std::string_view name) const;
    [[nodiscard]] const UiTextStyle *find_text_style(std::string_view name) const;
    [[nodiscard]] const UiMedallionStyle *find_medallion(std::string_view name) const;
    [[nodiscard]] const UiMedallionStyle *find_icon(std::string_view name) const;
    [[nodiscard]] const UiMedallionStyle *find_control_icon(std::string_view name) const;
    [[nodiscard]] int metric(std::string_view name, int fallback = 0) const;
    [[nodiscard]] std::optional<Color> find_color_role(std::string_view role) const;
    [[nodiscard]] std::optional<int> find_font_size_role(std::string_view role) const;
    [[nodiscard]] std::optional<int> find_spacing_role(std::string_view role) const;
    [[nodiscard]] std::optional<int> find_shape_role(std::string_view role) const;
    [[nodiscard]] std::optional<float> find_motion_role(std::string_view role) const;
};

} // namespace defn

#endif
