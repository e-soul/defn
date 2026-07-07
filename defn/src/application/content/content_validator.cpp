#include "content_validator.h"

#include "menu_models.h"
#include "progression_catalog.h"
#include "unit_data.h"
#include "upgrade_catalog.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace defn {

namespace {

bool contains_string(const std::vector<String> &values, const String &candidate) {
    return std::ranges::any_of(values, [&candidate](const String &value) { return value == candidate; });
}

std::string to_std_string(const String &value) { return value.utf8().get_data(); }

String to_godot_string(const std::string &value) { return {value.c_str()}; }

void push_issue(std::vector<String> &issues, const String &message) { issues.push_back(message); }

void validate_menu_content(const MenuContentData &menu_data, std::vector<String> &issues) {
    static const std::array<std::string, 4> required_menus = {"main_menu", "game_menu", "options_menu", "pause_menu"};
    for (const std::string &required_menu : required_menus) {
        if (menu_data.find_menu(required_menu) == nullptr) {
            push_issue(issues, vformat("menu_data.json missing required menu '%s'", to_godot_string(required_menu)));
        }
    }

    for (const auto &menu : menu_data.menus) {
        for (const auto &setting : menu.settings) {
            if (setting.kind == MenuSettingKind::UNKNOWN) {
                push_issue(issues, vformat("menu '%s' has unsupported setting '%s'", to_godot_string(menu.name), to_godot_string(setting.setting_id)));
            }
        }
    }
}

void validate_progression_catalog(const ProgressionCatalog &catalog, std::vector<String> &issues) {
    std::vector<String> level_ids;
    level_ids.reserve(catalog.get_level_unlocks().size());
    for (const auto &unlock : catalog.get_level_unlocks()) {
        if (unlock.level_id.is_empty()) {
            push_issue(issues, "progression.json contains a level unlock with an empty level_id");
            continue;
        }

        if (contains_string(level_ids, unlock.level_id)) {
            push_issue(issues, vformat("progression.json contains duplicate level_id '%s'", unlock.level_id));
        }
        level_ids.push_back(unlock.level_id);
    }

    for (const auto &unlock : catalog.get_level_unlocks()) {
        if (!unlock.requires_completed.is_empty() && !contains_string(level_ids, unlock.requires_completed)) {
            push_issue(issues, vformat("level '%s' requires unknown prerequisite level '%s'", unlock.level_id, unlock.requires_completed));
        }
    }
}

void validate_upgrade_catalog(const UpgradeCatalog &catalog, const UnitCatalog &unit_catalog, std::vector<String> &issues) {
    std::vector<String> card_ids;
    card_ids.reserve(catalog.get_cards().size());
    for (const auto &card : catalog.get_cards()) {
        if (contains_string(card_ids, card.id)) {
            push_issue(issues, vformat("upgrades.json contains duplicate upgrade id '%s'", card.id));
        }
        card_ids.push_back(card.id);
    }

    for (const auto &unit_id : catalog.get_base_units()) {
        const auto unit = unit_catalog.get_unit(to_std_string(unit_id));
        if (!unit.has_value()) {
            push_issue(issues, vformat("base unit '%s' does not exist in unit_data.json", unit_id));
        }
    }

    for (const auto &card : catalog.get_cards()) {
        for (const String &prerequisite : card.prerequisites) {
            if (!contains_string(card_ids, prerequisite)) {
                push_issue(issues, vformat("upgrade '%s' references unknown prerequisite '%s'", card.id, prerequisite));
            }
        }

        for (const auto &effect : card.effects) {
            const bool needs_unit = effect.type == UpgradeEffectType::UNIT_UNLOCK || effect.type == UpgradeEffectType::UNIT_HP_DELTA ||
                                    effect.type == UpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA || effect.type == UpgradeEffectType::UNIT_MOVE_SPEED_DELTA;
            if (needs_unit && !effect.unit_id.is_empty() && !unit_catalog.get_unit(to_std_string(effect.unit_id)).has_value()) {
                push_issue(issues, vformat("upgrade '%s' references unknown unit '%s'", card.id, effect.unit_id));
            }
        }
    }
}

void validate_levels(const ProgressionCatalog &catalog, const UnitCatalog &unit_catalog, const std::vector<LoadedLevelValidationInput> &levels,
                     std::vector<String> &issues) {
    for (const auto &unlock : catalog.get_level_unlocks()) {
        const auto found_level = std::ranges::find_if(levels, [&unlock](const LoadedLevelValidationInput &level) { return level.level_id == unlock.level_id; });
        if (found_level == levels.end() || !found_level->definition.has_value()) {
            push_issue(issues, vformat("missing or invalid level definition for '%s'", unlock.level_id));
            continue;
        }

        const LevelDefinition &level_definition = *found_level->definition;
        if (level_definition.waves.empty()) {
            push_issue(issues, vformat("level '%s' has no waves", unlock.level_id));
        }

        for (const auto &wave : level_definition.waves) {
            for (const auto &spawn : wave.spawns) {
                const auto unit = unit_catalog.get_unit(spawn.type);
                if (!unit.has_value()) {
                    push_issue(issues, vformat("level '%s' references unknown spawn type '%s'", unlock.level_id, to_godot_string(spawn.type)));
                    continue;
                }
                if (unit->side != UnitSide::HOSTILE) {
                    push_issue(issues, vformat("level '%s' uses non-hostile spawn type '%s'", unlock.level_id, to_godot_string(spawn.type)));
                }
            }
        }
    }
}

} // namespace

ContentValidationReport ContentValidator::validate_loaded_content(const std::optional<MenuContentData> &menu_data,
                                                                  const ProgressionCatalog *progression_catalog, const UpgradeCatalog *upgrade_catalog,
                                                                  const UnitCatalog *unit_catalog, const std::vector<LoadedLevelValidationInput> &levels) {
    ContentValidationReport report;

    if (menu_data.has_value()) {
        validate_menu_content(*menu_data, report.issues);
    }
    if (progression_catalog != nullptr) {
        validate_progression_catalog(*progression_catalog, report.issues);
    }
    if (upgrade_catalog != nullptr && unit_catalog != nullptr) {
        validate_upgrade_catalog(*upgrade_catalog, *unit_catalog, report.issues);
    }
    if (progression_catalog != nullptr && unit_catalog != nullptr) {
        validate_levels(*progression_catalog, *unit_catalog, levels, report.issues);
    }

    return report;
}

} // namespace defn