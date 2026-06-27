#ifndef CONTENT_VALIDATOR_H
#define CONTENT_VALIDATOR_H

#include "level_definition.h"
#include "menu_models.h"
#include "progression_catalog.h"
#include "unit_data.h"
#include "upgrade_catalog.h"

#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <vector>

namespace defn {

using namespace godot;

struct LoadedLevelValidationInput {
    String level_id;
    std::optional<LevelDefinition> definition;
};

struct ContentValidationReport {
    std::vector<String> issues;

    bool is_valid() const { return issues.empty(); }
};

class ContentValidator {
  public:
    ContentValidator() = delete;

    static ContentValidationReport validate_loaded_content(const std::optional<MenuContentData> &menu_data, const ProgressionCatalog *progression_catalog,
                                                           const UpgradeCatalog *upgrade_catalog, const UnitCatalog *unit_catalog,
                                                           const std::vector<LoadedLevelValidationInput> &levels);
    static bool report_startup_validation();
};

} // namespace defn

#endif