#include "content_validator.h"

#include "data_paths.h"
#include "level_loader.h"
#include "menu_data_loader.h"
#include "menu_models.h"
#include "progression_catalog.h"
#include "unit_data.h"
#include "upgrade_catalog.h"

#include <algorithm>
#include <optional>
#include <vector>

#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

bool contains_string(const std::vector<String> &values, const String &candidate) {
    return std::ranges::any_of(values, [&candidate](const String &value) { return value == candidate; });
}

void push_issue(std::vector<String> &issues, const String &message) { issues.push_back(message); }

void validate_menu_content(const MenuContentData &menu_data, std::vector<String> &issues) {
    static const std::array<String, 4> required_menus = {"main_menu", "game_menu", "options_menu", "pause_menu"};
    for (const String &required_menu : required_menus) {
        if (menu_data.find_menu(required_menu) == nullptr) {
            push_issue(issues, vformat("menu_data.json missing required menu '%s'", required_menu));
        }
    }

    for (const auto &menu : menu_data.menus) {
        for (const auto &setting : menu.settings) {
            if (setting.kind == MenuSettingKind::UNKNOWN) {
                push_issue(issues, vformat("menu '%s' has unsupported setting '%s'", menu.name, setting.setting_id));
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

void validate_upgrade_catalog(const UpgradeCatalog &catalog, const UnitDataLoader &unit_data, std::vector<String> &issues) {
    std::vector<String> card_ids;
    card_ids.reserve(catalog.get_cards().size());
    for (const auto &card : catalog.get_cards()) {
        if (contains_string(card_ids, card.id)) {
            push_issue(issues, vformat("upgrades.json contains duplicate upgrade id '%s'", card.id));
        }
        card_ids.push_back(card.id);
    }

    for (const auto &unit_id : catalog.get_base_units()) {
        const auto unit = unit_data.get_unit(unit_id);
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
            if (needs_unit && !effect.unit_id.is_empty() && !unit_data.get_unit(effect.unit_id).has_value()) {
                push_issue(issues, vformat("upgrade '%s' references unknown unit '%s'", card.id, effect.unit_id));
            }
        }
    }
}

void validate_levels(const ProgressionCatalog &catalog, const UnitDataLoader &unit_data, std::vector<String> &issues) {
    for (const auto &unlock : catalog.get_level_unlocks()) {
        const String level_path = DataPaths::level_definition(unlock.level_id);
        const auto level_definition = LevelLoader::load(level_path);
        if (!level_definition) {
            push_issue(issues, vformat("missing or invalid level definition for '%s'", unlock.level_id));
            continue;
        }

        if (level_definition->waves.empty()) {
            push_issue(issues, vformat("level '%s' has no waves", unlock.level_id));
        }

        for (const auto &wave : level_definition->waves) {
            for (const auto &spawn : wave.spawns) {
                const auto unit = unit_data.get_unit(spawn.type);
                if (!unit.has_value()) {
                    push_issue(issues, vformat("level '%s' references unknown spawn type '%s'", unlock.level_id, spawn.type));
                    continue;
                }
                if (unit->side != UnitSide::HOSTILE) {
                    push_issue(issues, vformat("level '%s' uses non-hostile spawn type '%s'", unlock.level_id, spawn.type));
                }
            }
        }
    }
}

} // namespace

bool ContentValidator::report_startup_validation() {
    static std::optional<bool> cached_result;
    if (cached_result.has_value()) {
        return *cached_result;
    }

    std::vector<String> issues;

    const auto menu_data = MenuDataLoader::load(DataPaths::MENU_DATA);
    if (!menu_data) {
        push_issue(issues, "failed to load menu_data.json");
    }

    ProgressionCatalog progression_catalog;
    const bool progression_loaded = progression_catalog.load(DataPaths::PROGRESSION_DATA);
    if (!progression_loaded) {
        push_issue(issues, "failed to load progression.json");
    }

    UpgradeCatalog upgrade_catalog;
    const bool upgrades_loaded = upgrade_catalog.load(DataPaths::UPGRADES_DATA);
    if (!upgrades_loaded) {
        push_issue(issues, "failed to load upgrades.json");
    }

    UnitDataLoader unit_data;
    const bool units_loaded = unit_data.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS);
    if (!units_loaded) {
        push_issue(issues, "failed to load unit_data.json or unit_globals.json");
    }

    if (menu_data) {
        validate_menu_content(*menu_data, issues);
    }
    if (progression_loaded) {
        validate_progression_catalog(progression_catalog, issues);
    }
    if (upgrades_loaded && units_loaded) {
        validate_upgrade_catalog(upgrade_catalog, unit_data, issues);
    }
    if (progression_loaded && units_loaded) {
        validate_levels(progression_catalog, unit_data, issues);
    }

    if (issues.empty()) {
        UtilityFunctions::print("ContentValidator: content validation passed");
        cached_result = true;
        return true;
    }

    UtilityFunctions::printerr("ContentValidator: content validation found ", static_cast<int>(issues.size()), " issue(s)");
    for (const String &issue : issues) {
        UtilityFunctions::printerr("  - ", issue);
    }

    cached_result = false;
    return false;
}

} // namespace defn