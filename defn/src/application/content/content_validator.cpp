#include "content_validator.h"

#include <algorithm>
#include <array>

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

} // namespace

ContentValidationReport ContentValidator::validate_loaded_content(const ContentValidationInput &input) {
    ContentValidationReport report;

    if (input.menu_data.has_value()) {
        validate_menu_content(*input.menu_data, report.issues);
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
