// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_theme_models.h"

#include <array>
#include <utility>

namespace defn {

namespace {

template <typename Map> const typename Map::mapped_type *find_entry(const Map &entries, std::string_view name) {
    const auto found = entries.find(name);
    return found == entries.end() ? nullptr : &found->second;
}

} // namespace

const UiSurfaceStyle *UiThemeData::find_surface(std::string_view name) const { return find_entry(surfaces, name); }

const UiButtonVariant *UiThemeData::find_button(std::string_view name) const { return find_entry(buttons, name); }

const UiTextStyle *UiThemeData::find_text_style(std::string_view name) const { return find_entry(text_styles, name); }

const UiMedallionStyle *UiThemeData::find_medallion(std::string_view name) const { return find_entry(medallions, name); }

int UiThemeData::metric(std::string_view name, int fallback) const {
    const auto found = metrics.find(name);
    return found == metrics.end() ? fallback : found->second;
}

std::optional<Color> UiThemeData::find_color_role(std::string_view role) const {
    const std::array<std::pair<std::string_view, const Color *>, 33> roles = {{
        {"surface", &palette.surface},
        {"surface_raised", &palette.surface_raised},
        {"surface_sunken", &palette.surface_sunken},
        {"overlay_scrim", &palette.overlay_scrim},
        {"border", &palette.border},
        {"border_strong", &palette.border_strong},
        {"border_focus", &palette.border_focus},
        {"accent", &palette.accent},
        {"accent_strong", &palette.accent_strong},
        {"text_primary", &palette.text_primary},
        {"text_secondary", &palette.text_secondary},
        {"text_muted", &palette.text_muted},
        {"text_inverse", &palette.text_inverse},
        {"state_success", &palette.state_success},
        {"state_warning", &palette.state_warning},
        {"state_danger", &palette.state_danger},
        {"state_locked", &palette.state_locked},
        {"energy", &palette.energy},
        {"victory", &palette.victory},
        {"defeat", &palette.defeat},
        {"overlay_victory", &palette.overlay_victory},
        {"overlay_defeat", &palette.overlay_defeat},
        {"backdrop", &palette.backdrop},
        {"scrim_soft", &palette.scrim_soft},
        {"scrim_panel", &palette.scrim_panel},
        {"route_locked", &palette.route_locked},
        {"locked_tint", &palette.locked_tint},
        {"ambience_dust", &palette.ambience_dust},
        {"ambience_spores", &palette.ambience_spores},
        {"ambience_mist", &palette.ambience_mist},
        {"ambience_snow", &palette.ambience_snow},
        {"ambience_embers", &palette.ambience_embers},
        {"transparent", &palette.transparent},
    }};

    for (const auto &[name, color] : roles) {
        if (name == role) {
            return *color;
        }
    }
    if (const auto found = palette.extra.find(role); found != palette.extra.end()) {
        return found->second;
    }
    return std::nullopt;
}

std::optional<int> UiThemeData::find_font_size_role(std::string_view role) const {
    const std::array<std::pair<std::string_view, int>, 12> roles = {{
        {"banner", typography.banner},
        {"display", typography.display},
        {"title", typography.title},
        {"menu", typography.menu},
        {"section", typography.section},
        {"heading", typography.heading},
        {"stat", typography.stat},
        {"subheading", typography.subheading},
        {"body", typography.body},
        {"caption", typography.caption},
        {"card_body", typography.card_body},
        {"micro", typography.micro},
    }};

    for (const auto &[name, size] : roles) {
        if (name == role) {
            return size;
        }
    }
    return std::nullopt;
}

std::optional<int> UiThemeData::find_spacing_role(std::string_view role) const {
    const std::array<std::pair<std::string_view, int>, 7> roles = {{
        {"xs", spacing.xs},
        {"sm", spacing.sm},
        {"md", spacing.md},
        {"lg", spacing.lg},
        {"xl", spacing.xl},
        {"screen_margin", spacing.screen_margin},
        {"section_gap", spacing.section_gap},
    }};

    for (const auto &[name, value] : roles) {
        if (name == role) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<int> UiThemeData::find_shape_role(std::string_view role) const {
    const std::array<std::pair<std::string_view, int>, 6> roles = {{
        {"corner_sm", shape.corner_sm},
        {"corner_md", shape.corner_md},
        {"corner_lg", shape.corner_lg},
        {"corner_pill", shape.corner_pill},
        {"border_width", shape.border_width},
        {"border_width_strong", shape.border_width_strong},
    }};

    for (const auto &[name, value] : roles) {
        if (name == role) {
            return value;
        }
    }
    return std::nullopt;
}

} // namespace defn
