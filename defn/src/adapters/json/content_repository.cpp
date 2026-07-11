#include "content_repository.h"

#include "data_paths.h"
#include "godot_string.h"
#include "level_loader.h"
#include "menu_data_loader.h"

namespace defn {

JsonContentPaths default_json_content_paths() {
    return {
        .menu_path = DataPaths::MENU_DATA,
        .progression_path = DataPaths::PROGRESSION_DATA,
        .upgrades_path = DataPaths::UPGRADES_DATA,
        .unit_path = DataPaths::UNIT_DATA,
        .unit_globals_path = DataPaths::UNIT_GLOBALS,
        .levels_directory = DataPaths::LEVELS_DIRECTORY,
    };
}

JsonContentRepository::JsonContentRepository(JsonContentPaths paths) : paths_(std::move(paths)) {}

JsonLoadedContent JsonContentRepository::load_for_validation() const {
    JsonLoadedContent content;

    content.menu_data = MenuDataLoader::load(paths_.menu_path);
    if (!content.menu_data.has_value()) {
        content.load_issues.emplace_back("failed to load menu_data.json");
    }

    content.progression_loaded = content.progression_catalog.load(paths_.progression_path);
    if (!content.progression_loaded) {
        content.load_issues.emplace_back("failed to load progression.json");
    }

    content.upgrades_loaded = content.upgrade_catalog.load(paths_.upgrades_path);
    if (!content.upgrades_loaded) {
        content.load_issues.emplace_back("failed to load upgrades.json");
    }

    content.units_loaded = content.unit_data.load(paths_.unit_path, paths_.unit_globals_path);
    if (!content.units_loaded) {
        content.load_issues.emplace_back("failed to load unit_data.json or unit_globals.json");
    }

    if (content.progression_loaded) {
        content.levels.reserve(content.progression_catalog.get_level_unlocks().size());
        for (const auto &unlock : content.progression_catalog.get_level_unlocks()) {
            content.levels.push_back({
                .level_id = to_std_string(unlock.level_id),
                .definition = content.units_loaded ? LevelLoader::load(level_definition_path(unlock.level_id)) : std::nullopt,
            });
        }
    }

    return content;
}

String JsonContentRepository::level_definition_path(const String &level_id) const { return vformat("%s/%s.json", paths_.levels_directory, level_id); }

ContentValidationInput make_content_validation_input(const std::optional<MenuContentData> &menu_data, const ProgressionCatalog *progression_catalog,
                                                     const UpgradeCatalog *upgrade_catalog, const UnitCatalog *unit_catalog,
                                                     std::vector<LoadedLevelValidationInput> levels) {
    ContentValidationInput input;
    input.menu_data = menu_data;
    input.unit_catalog = unit_catalog;
    input.levels = std::move(levels);

    if (progression_catalog != nullptr) {
        ProgressionCatalogValidationData progression;
        progression.level_unlocks.reserve(progression_catalog->get_level_unlocks().size());
        for (const auto &unlock : progression_catalog->get_level_unlocks()) {
            progression.level_unlocks.push_back({
                .level_id = to_std_string(unlock.level_id),
                .requires_completed = to_std_string(unlock.requires_completed),
            });
        }
        input.progression_catalog = std::move(progression);
    }

    if (upgrade_catalog != nullptr) {
        UpgradeCatalogValidationData upgrades;
        upgrades.base_units.reserve(upgrade_catalog->get_base_units().size());
        for (const String &unit_id : upgrade_catalog->get_base_units()) {
            upgrades.base_units.push_back(to_std_string(unit_id));
        }
        upgrades.cards.reserve(upgrade_catalog->get_cards().size());
        for (const auto &card : upgrade_catalog->get_cards()) {
            UpgradeCardValidationData validation_card;
            validation_card.id = to_std_string(card.id);
            validation_card.prerequisites.reserve(card.prerequisites.size());
            for (const String &prerequisite : card.prerequisites) {
                validation_card.prerequisites.push_back(to_std_string(prerequisite));
            }
            validation_card.effects.reserve(card.effects.size());
            for (const auto &effect : card.effects) {
                const bool requires_known_unit = effect.type == UpgradeEffectType::UNIT_UNLOCK || effect.type == UpgradeEffectType::UNIT_HP_DELTA ||
                                                 effect.type == UpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA ||
                                                 effect.type == UpgradeEffectType::UNIT_MOVE_SPEED_DELTA;
                validation_card.effects.push_back({
                    .unit_id = to_std_string(effect.unit_id),
                    .requires_known_unit = requires_known_unit,
                });
            }
            upgrades.cards.push_back(std::move(validation_card));
        }
        input.upgrade_catalog = std::move(upgrades);
    }

    return input;
}

ContentValidationInput make_content_validation_input(const JsonLoadedContent &content) {
    ContentValidationInput input = make_content_validation_input(content.menu_data, content.progression_loaded ? &content.progression_catalog : nullptr,
                                                                 content.upgrades_loaded ? &content.upgrade_catalog : nullptr,
                                                                 content.units_loaded ? &content.unit_data : nullptr, content.levels);
    if (content.units_loaded) {
        input.field_promotion_rules = content.unit_data.get_globals().field_promotion;
    }
    return input;
}

} // namespace defn
