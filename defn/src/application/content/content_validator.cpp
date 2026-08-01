// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "content_validator.h"

#include <algorithm>
#include <array>
#include <unordered_set>

namespace defn {

namespace {

bool contains_string(const std::vector<std::string> &values, const std::string &candidate) {
    return std::ranges::any_of(values, [&candidate](const std::string &value) { return value == candidate; });
}

void push_issue(std::vector<std::string> &issues, std::string message) { issues.push_back(std::move(message)); }

std::string quoted(const std::string &value) { return "'" + value + "'"; }

void validate_menu_content(const MenuContentData &menu_data, std::vector<std::string> &issues) {
    static const std::array<std::string, 4> required_menus = {"main_menu", "game_menu", "options_menu", "pause_menu"};
    for (const std::string &required_menu : required_menus) {
        if (menu_data.find_menu(required_menu) == nullptr) {
            push_issue(issues, "menu_data.json missing required menu " + quoted(required_menu));
        }
    }

    for (const auto &menu : menu_data.menus) {
        for (const auto &setting : menu.settings) {
            if (setting.kind == MenuSettingKind::UNKNOWN) {
                push_issue(issues, "menu " + quoted(menu.name) + " has unsupported setting " + quoted(setting.setting_id));
            }
        }
    }
}

bool normalized(float value) { return value >= 0.0F && value <= 1.0F; }

void validate_texture(const CampaignTextureDefinition &texture, const std::string &label, std::vector<std::string> &issues) {
    if (texture.path.empty()) {
        push_issue(issues, "campaign map " + label + " path is empty");
    }
}

void validate_preview(const CampaignPreviewDefinition &preview, const std::string &label, std::vector<std::string> &issues) {
    validate_texture(preview.texture, label, issues);
    if (!normalized(preview.focus_x) || !normalized(preview.focus_y)) {
        push_issue(issues, "campaign map " + label + " focus must be normalized");
    }
    if (preview.node_zoom < 1.0F || preview.dossier_zoom < 1.0F) {
        push_issue(issues, "campaign map " + label + " zoom values must be at least 1.0");
    }
}

std::vector<std::string> validate_campaign_missions(const CampaignMapDefinition &map, std::vector<std::string> &issues) {
    static const std::unordered_set<std::string> threats = {"low", "moderate", "high", "severe", "extreme"};
    std::vector<std::string> mission_ids;
    mission_ids.reserve(map.missions.size());
    for (const auto &mission : map.missions) {
        if (contains_string(mission_ids, mission.level_id)) {
            push_issue(issues, "campaign_map.json contains duplicate level_id " + quoted(mission.level_id));
        }
        mission_ids.push_back(mission.level_id);
        if (!normalized(mission.position_normalized.x) || !normalized(mission.position_normalized.y)) {
            push_issue(issues, "campaign map position for " + quoted(mission.level_id) + " must be normalized");
        }
        if (!threats.contains(mission.threat_id)) {
            push_issue(issues, "campaign map mission " + quoted(mission.level_id) + " has unknown threat " + quoted(mission.threat_id));
        }
        if (mission.ambience == CampaignMapAmbience::UNKNOWN) {
            push_issue(issues, "campaign map mission " + quoted(mission.level_id) + " has unknown ambience");
        }
        validate_preview(mission.preview, "preview for " + quoted(mission.level_id), issues);
    }
    return mission_ids;
}

void validate_campaign_progression(const std::vector<std::string> &mission_ids, const ProgressionCatalogValidationData &progression,
                                   std::vector<std::string> &issues) {
    for (std::size_t index = 0; index < progression.level_unlocks.size(); ++index) {
        const std::string &expected = progression.level_unlocks[index].level_id;
        if (!contains_string(mission_ids, expected)) {
            push_issue(issues, "campaign_map.json missing progression level " + quoted(expected));
        } else if (index >= mission_ids.size() || mission_ids[index] != expected) {
            push_issue(issues, "campaign map mission order must follow progression.json");
        }
    }
    for (const std::string &mission_id : mission_ids) {
        const bool known =
            std::ranges::any_of(progression.level_unlocks, [&mission_id](const ProgressionLevelValidationData &level) { return level.level_id == mission_id; });
        if (!known) {
            push_issue(issues, "campaign_map.json contains unknown level " + quoted(mission_id));
        }
    }
}

void validate_campaign_map(const CampaignMapDefinition &map, const ProgressionCatalogValidationData *progression,
                           const std::vector<std::string> &missing_assets, std::vector<std::string> &issues) {
    validate_texture(map.background, "background", issues);
    const std::vector<std::string> mission_ids = validate_campaign_missions(map, issues);
    if (progression != nullptr) {
        validate_campaign_progression(mission_ids, *progression, issues);
    }

    for (const std::string &asset : missing_assets) {
        push_issue(issues, "campaign map resource does not exist: " + quoted(asset));
    }
}

void validate_progression_catalog(const ProgressionCatalogValidationData &catalog, std::vector<std::string> &issues) {
    std::vector<std::string> level_ids;
    level_ids.reserve(catalog.level_unlocks.size());
    for (const auto &unlock : catalog.level_unlocks) {
        if (unlock.level_id.empty()) {
            push_issue(issues, "progression.json contains a level unlock with an empty level_id");
            continue;
        }

        if (contains_string(level_ids, unlock.level_id)) {
            push_issue(issues, "progression.json contains duplicate level_id " + quoted(unlock.level_id));
        }
        level_ids.push_back(unlock.level_id);
    }

    for (const auto &unlock : catalog.level_unlocks) {
        if (!unlock.requires_completed.empty() && !contains_string(level_ids, unlock.requires_completed)) {
            push_issue(issues, "level " + quoted(unlock.level_id) + " requires unknown prerequisite level " + quoted(unlock.requires_completed));
        }
    }
}

void validate_upgrade_catalog(const UpgradeCatalogValidationData &catalog, const UnitCatalog &unit_catalog, std::vector<std::string> &issues) {
    std::vector<std::string> card_ids;
    card_ids.reserve(catalog.cards.size());
    for (const auto &card : catalog.cards) {
        if (contains_string(card_ids, card.id)) {
            push_issue(issues, "upgrades.json contains duplicate upgrade id " + quoted(card.id));
        }
        card_ids.push_back(card.id);
    }

    for (const auto &unit_id : catalog.base_units) {
        if (!unit_catalog.get_unit(unit_id).has_value()) {
            push_issue(issues, "base unit " + quoted(unit_id) + " does not exist in unit_data.json");
        }
    }

    for (const auto &card : catalog.cards) {
        for (const std::string &prerequisite : card.prerequisites) {
            if (!contains_string(card_ids, prerequisite)) {
                push_issue(issues, "upgrade " + quoted(card.id) + " references unknown prerequisite " + quoted(prerequisite));
            }
        }

        for (const auto &effect : card.effects) {
            if (effect.requires_known_unit && !effect.unit_id.empty() && !unit_catalog.get_unit(effect.unit_id).has_value()) {
                push_issue(issues, "upgrade " + quoted(card.id) + " references unknown unit " + quoted(effect.unit_id));
            }
        }
    }
}

void validate_levels(const ProgressionCatalogValidationData &catalog, const UnitCatalog &unit_catalog, const std::vector<LoadedLevelValidationInput> &levels,
                     std::vector<std::string> &issues) {
    for (const auto &unlock : catalog.level_unlocks) {
        const auto found_level = std::ranges::find_if(levels, [&unlock](const LoadedLevelValidationInput &level) { return level.level_id == unlock.level_id; });
        if (found_level == levels.end() || !found_level->definition.has_value()) {
            push_issue(issues, "missing or invalid level definition for " + quoted(unlock.level_id));
            continue;
        }

        const LevelDefinition &level_definition = *found_level->definition;
        if (!normalized(level_definition.base_position_ratio.x) || !normalized(level_definition.base_position_ratio.y)) {
            push_issue(issues, "level " + quoted(unlock.level_id) + " base_position must be normalized");
        }
        if (!normalized(level_definition.belt_width_ratio.x) || !normalized(level_definition.belt_width_ratio.y) ||
            level_definition.belt_width_ratio.x == level_definition.belt_width_ratio.y) {
            push_issue(issues, "level " + quoted(unlock.level_id) + " belt_width must contain two distinct normalized ratios");
        }
        if (level_definition.waves.empty()) {
            push_issue(issues, "level " + quoted(unlock.level_id) + " has no waves");
        }

        for (const auto &wave : level_definition.waves) {
            for (const auto &spawn : wave.spawns) {
                const auto unit = unit_catalog.get_unit(spawn.type);
                if (!unit.has_value()) {
                    push_issue(issues, "level " + quoted(unlock.level_id) + " references unknown spawn type " + quoted(spawn.type));
                    continue;
                }
                if (unit->side != UnitSide::HOSTILE) {
                    push_issue(issues, "level " + quoted(unlock.level_id) + " uses non-hostile spawn type " + quoted(spawn.type));
                }
            }
        }
    }
}

void require_color_role(const UiThemeData &theme, const std::string &role, const std::string &owner, std::vector<std::string> &issues) {
    if (!role.empty() && !theme.find_color_role(role).has_value()) {
        push_issue(issues, "ui_theme.json " + owner + " references unknown color role " + quoted(role));
    }
}

void require_font_size_role(const UiThemeData &theme, const std::string &role, const std::string &owner, std::vector<std::string> &issues) {
    if (!role.empty() && !theme.find_font_size_role(role).has_value()) {
        push_issue(issues, "ui_theme.json " + owner + " references unknown font size role " + quoted(role));
    }
}

void require_spacing_role(const UiThemeData &theme, const std::string &role, const std::string &owner, std::vector<std::string> &issues) {
    if (!role.empty() && !theme.find_spacing_role(role).has_value()) {
        push_issue(issues, "ui_theme.json " + owner + " references unknown spacing role " + quoted(role));
    }
}

void require_shape_role(const UiThemeData &theme, const std::string &role, const std::string &owner, std::vector<std::string> &issues) {
    if (!role.empty() && !theme.find_shape_role(role).has_value()) {
        push_issue(issues, "ui_theme.json " + owner + " references unknown shape role " + quoted(role));
    }
}

void validate_button_state(const UiThemeData &theme, const UiButtonState &state, const std::string &owner, std::vector<std::string> &issues) {
    require_color_role(theme, state.bg_role, owner, issues);
    require_color_role(theme, state.border_role, owner, issues);
    require_color_role(theme, state.font_role, owner, issues);
}

void validate_ui_theme(const UiThemeData &theme, std::vector<std::string> &issues) {
    for (const auto &[name, surface] : theme.surfaces) {
        const std::string owner = "surface " + quoted(name);
        require_color_role(theme, surface.bg_role, owner, issues);
        require_color_role(theme, surface.border_role, owner, issues);
        require_color_role(theme, surface.shadow_role, owner, issues);
        require_shape_role(theme, surface.shape_role, owner, issues);
        require_shape_role(theme, surface.border_width_role, owner, issues);
        require_spacing_role(theme, surface.content_margin_role, owner, issues);
    }

    for (const auto &[name, button] : theme.buttons) {
        const std::string owner = "button " + quoted(name);
        require_font_size_role(theme, button.font_size_role, owner, issues);
        require_shape_role(theme, button.shape_role, owner, issues);
        require_spacing_role(theme, button.content_margin_role, owner, issues);
        validate_button_state(theme, button.normal, owner, issues);
        validate_button_state(theme, button.hover, owner, issues);
        validate_button_state(theme, button.pressed, owner, issues);
        validate_button_state(theme, button.disabled, owner, issues);
        validate_button_state(theme, button.focus, owner, issues);
    }

    for (const auto &[name, text_style] : theme.text_styles) {
        const std::string owner = "text style " + quoted(name);
        require_font_size_role(theme, text_style.font_size_role, owner, issues);
        require_color_role(theme, text_style.color_role, owner, issues);
        if (text_style.outline_size > 0) {
            require_color_role(theme, text_style.outline_role, owner, issues);
        }
    }

    const std::string screen_owner = "screen";
    require_color_role(theme, theme.screen.backdrop_role, screen_owner, issues);
    require_spacing_role(theme, theme.screen.footer_gap_role, screen_owner, issues);
    if (theme.find_text_style(theme.screen.title_text_style) == nullptr) {
        push_issue(issues, "ui_theme.json screen references unknown text style " + quoted(theme.screen.title_text_style));
    }
    if (theme.find_text_style(theme.screen.subtitle_text_style) == nullptr) {
        push_issue(issues, "ui_theme.json screen references unknown text style " + quoted(theme.screen.subtitle_text_style));
    }
    if (theme.find_surface(theme.screen.panel_surface) == nullptr) {
        push_issue(issues, "ui_theme.json screen references unknown surface " + quoted(theme.screen.panel_surface));
    }
}

} // namespace

ContentValidationReport ContentValidator::validate_loaded_content(const ContentValidationInput &input) {
    ContentValidationReport report;

    if (input.field_promotion_rules.has_value()) {
        const FieldPromotionRules &rules = *input.field_promotion_rules;
        if (rules.damage_threshold <= 0) {
            push_issue(report.issues, "field promotion damage_threshold must be positive");
        }
        if (rules.damage_multiplier < 1.0) {
            push_issue(report.issues, "field promotion damage_multiplier must be at least 1.0");
        }
        if (rules.attack_period_multiplier <= 0.0 || rules.attack_period_multiplier > 1.0) {
            push_issue(report.issues, "field promotion attack_period_multiplier must be in (0, 1]");
        }
        if (rules.health_multiplier < 1.0) {
            push_issue(report.issues, "field promotion health_multiplier must be at least 1.0");
        }
    }

    if (input.campaign_map.has_value()) {
        validate_campaign_map(*input.campaign_map, input.progression_catalog.has_value() ? &*input.progression_catalog : nullptr, input.missing_campaign_assets,
                              report.issues);
    }
    if (input.menu_data.has_value()) {
        validate_menu_content(*input.menu_data, report.issues);
    }
    if (input.ui_theme.has_value()) {
        validate_ui_theme(*input.ui_theme, report.issues);
    }
    if (input.progression_catalog.has_value()) {
        validate_progression_catalog(*input.progression_catalog, report.issues);
    }
    if (input.upgrade_catalog.has_value() && input.unit_catalog != nullptr) {
        validate_upgrade_catalog(*input.upgrade_catalog, *input.unit_catalog, report.issues);
    }
    if (input.progression_catalog.has_value() && input.unit_catalog != nullptr) {
        validate_levels(*input.progression_catalog, *input.unit_catalog, input.levels, report.issues);
    }

    return report;
}

} // namespace defn
