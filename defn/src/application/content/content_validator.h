#ifndef CONTENT_VALIDATOR_H
#define CONTENT_VALIDATOR_H

#include "level_definition.h"
#include "menu_models.h"
#include "unit_definition.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

struct LoadedLevelValidationInput {
    std::string level_id;
    std::optional<LevelDefinition> definition;
};

struct ProgressionLevelValidationData {
    std::string level_id;
    std::string requires_completed;
};

struct ProgressionCatalogValidationData {
    std::vector<ProgressionLevelValidationData> level_unlocks;
};

struct UpgradeEffectValidationData {
    std::string unit_id;
    bool requires_known_unit = false;
};

struct UpgradeCardValidationData {
    std::string id;
    std::vector<std::string> prerequisites;
    std::vector<UpgradeEffectValidationData> effects;
};

struct UpgradeCatalogValidationData {
    std::vector<std::string> base_units;
    std::vector<UpgradeCardValidationData> cards;
};

struct ContentValidationInput {
    std::optional<MenuContentData> menu_data;
    std::optional<ProgressionCatalogValidationData> progression_catalog;
    std::optional<UpgradeCatalogValidationData> upgrade_catalog;
    const UnitCatalog *unit_catalog = nullptr;
    std::optional<FieldPromotionRules> field_promotion_rules;
    std::vector<LoadedLevelValidationInput> levels;
};

struct ContentValidationReport {
    std::vector<std::string> issues;

    bool is_valid() const { return issues.empty(); }
};

class ContentValidator {
  public:
    ContentValidator() = delete;

    static ContentValidationReport validate_loaded_content(const ContentValidationInput &input);
};

} // namespace defn

#endif
