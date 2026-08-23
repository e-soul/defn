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
};

struct UiPalette {
    Color surface = {0.035F, 0.072F, 0.110F, 0.96F};
    Color surface_raised = {0.100F, 0.160F, 0.200F, 0.94F};
    Color surface_sunken = {0.020F, 0.040F, 0.062F, 0.98F};
    Color overlay_scrim = {0.0F, 0.0F, 0.0F, 0.70F};
    Color border = {0.345F, 0.396F, 0.416F, 1.0F};
    Color border_strong = {0.520F, 0.580F, 0.600F, 1.0F};
    Color border_focus = {0.969F, 0.898F, 0.627F, 1.0F};
    Color accent = {0.949F, 0.745F, 0.333F, 1.0F};
    Color accent_strong = {0.969F, 0.898F, 0.627F, 1.0F};
    Color text_primary = {0.910F, 0.867F, 0.765F, 1.0F};
    Color text_secondary = {0.725F, 0.769F, 0.765F, 1.0F};
    Color text_muted = {0.620F, 0.678F, 0.682F, 1.0F};
    Color text_inverse = {0.039F, 0.067F, 0.094F, 1.0F};
    Color state_success = {0.373F, 0.796F, 0.604F, 1.0F};
    Color state_warning = {0.949F, 0.745F, 0.333F, 1.0F};
    Color state_danger = {0.843F, 0.639F, 0.608F, 1.0F};
    Color state_locked = {0.349F, 0.392F, 0.424F, 1.0F};
    Color energy = {0.300F, 0.700F, 1.000F, 1.0F};
    Color victory = {0.373F, 0.796F, 0.604F, 1.0F};
    Color defeat = {0.843F, 0.290F, 0.290F, 1.0F};
    Color overlay_victory = {0.030F, 0.120F, 0.080F, 0.42F};
    Color overlay_defeat = {0.180F, 0.000F, 0.000F, 0.48F};
    Color backdrop = {0.039F, 0.067F, 0.094F, 1.0F};
    Color scrim_soft = {0.010F, 0.025F, 0.035F, 0.18F};
    Color scrim_panel = {0.020F, 0.035F, 0.050F, 0.50F};
    Color route_locked = {0.350F, 0.390F, 0.420F, 0.48F};
    Color locked_tint = {0.550F, 0.600F, 0.650F, 0.45F};
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

struct UiButtonVariant {
    int min_width = 0;
    int min_height = 0;
    std::string font_size_role = "body";
    std::string shape_role = "corner_md";
    std::string content_margin_role;
    UiButtonState normal;
    UiButtonState hover;
    UiButtonState pressed;
    UiButtonState disabled;
    UiButtonState focus;
};

struct UiTextStyle {
    std::string font_size_role = "body";
    std::string color_role = "text_primary";
    int outline_size = 0;
    std::string outline_role;
};

/// A tinted vector mark inside a round plate: the SVG texture and the palette role it is modulated with.
/// Shared by the campaign map node states and the HUD instrument icons.
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
    std::string font_path;
    std::map<std::string, UiSurfaceStyle, std::less<>> surfaces;
    std::map<std::string, UiButtonVariant, std::less<>> buttons;
    std::map<std::string, UiTextStyle, std::less<>> text_styles;
    std::map<std::string, UiMedallionStyle, std::less<>> medallions;
    std::map<std::string, UiMedallionStyle, std::less<>> hud_icons;
    std::map<std::string, int, std::less<>> metrics;
    UiScreenStyle screen;
    UiSfxData sfx;

    [[nodiscard]] const UiSurfaceStyle *find_surface(std::string_view name) const;
    [[nodiscard]] const UiButtonVariant *find_button(std::string_view name) const;
    [[nodiscard]] const UiTextStyle *find_text_style(std::string_view name) const;
    [[nodiscard]] const UiMedallionStyle *find_medallion(std::string_view name) const;
    [[nodiscard]] const UiMedallionStyle *find_hud_icon(std::string_view name) const;
    [[nodiscard]] int metric(std::string_view name, int fallback = 0) const;
    [[nodiscard]] std::optional<Color> find_color_role(std::string_view role) const;
    [[nodiscard]] std::optional<int> find_font_size_role(std::string_view role) const;
    [[nodiscard]] std::optional<int> find_spacing_role(std::string_view role) const;
    [[nodiscard]] std::optional<int> find_shape_role(std::string_view role) const;
};

} // namespace defn

#endif
