#include "content_repository.h"

#include "data_paths.h"
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
                .level_id = unlock.level_id,
                .definition = content.units_loaded ? LevelLoader::load(level_definition_path(unlock.level_id)) : std::nullopt,
            });
        }
    }

    return content;
}

String JsonContentRepository::level_definition_path(const String &level_id) const { return vformat("%s/%s.json", paths_.levels_directory, level_id); }

} // namespace defn