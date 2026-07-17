#ifndef CONTENT_REPOSITORY_H
#define CONTENT_REPOSITORY_H

#include "content_validator.h"
#include "menu_models.h"
#include "music_playlist.h"
#include "progression_catalog.h"
#include "unit_data.h"
#include "upgrade_catalog.h"

#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <vector>

namespace defn {

using namespace godot;

struct JsonContentPaths {
    String menu_path;
    String music_playlist_path;
    String progression_path;
    String upgrades_path;
    String unit_path;
    String unit_globals_path;
    String levels_directory;
};

struct JsonLoadedContent {
    std::optional<MenuContentData> menu_data;
    std::optional<MusicPlaylist> music_playlist;
    ProgressionCatalog progression_catalog;
    UpgradeCatalog upgrade_catalog;
    UnitDataLoader unit_data;
    bool progression_loaded = false;
    bool upgrades_loaded = false;
    bool units_loaded = false;
    std::vector<LoadedLevelValidationInput> levels;
    std::vector<String> load_issues;
};

class JsonContentRepository {
  public:
    explicit JsonContentRepository(JsonContentPaths paths);

    [[nodiscard]] JsonLoadedContent load_for_validation() const;

  private:
    [[nodiscard]] String level_definition_path(const String &level_id) const;

    JsonContentPaths paths_;
};

[[nodiscard]] JsonContentPaths default_json_content_paths();
[[nodiscard]] ContentValidationInput make_content_validation_input(const std::optional<MenuContentData> &menu_data,
                                                                   const ProgressionCatalog *progression_catalog, const UpgradeCatalog *upgrade_catalog,
                                                                   const UnitCatalog *unit_catalog, std::vector<LoadedLevelValidationInput> levels);
[[nodiscard]] ContentValidationInput make_content_validation_input(const JsonLoadedContent &content);

} // namespace defn

#endif
