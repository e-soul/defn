#include "test_harness.h"

#include "content_validator.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

namespace {

class FakeUnitCatalog final : public UnitCatalog {
  public:
    std::vector<UnitConfig> units;

    [[nodiscard]] std::optional<UnitConfig> get_unit(const std::string &name) const override {
        for (const UnitConfig &unit : units) {
            if (unit.name == name) {
                return unit;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<UnitConfig> get_friendly_units() const override {
        std::vector<UnitConfig> friendlies;
        for (const UnitConfig &unit : units) {
            if (unit.side == UnitSide::FRIENDLY) {
                friendlies.push_back(unit);
            }
        }
        return friendlies;
    }
};

bool contains_issue(const ContentValidationReport &report, const std::string &needle) {
    for (const std::string &issue : report.issues) {
        if (issue.contains(needle)) {
            return true;
        }
    }
    return false;
}

ContentValidationInput make_valid_input(FakeUnitCatalog &units) {
    units.units = {
        {.name = "operator", .side = UnitSide::FRIENDLY},
        {.name = "jackal", .side = UnitSide::HOSTILE},
    };

    ContentValidationInput input;
    input.menu_data = MenuContentData{
        .menus = {{.name = "main_menu"}, {.name = "game_menu"}, {.name = "options_menu"}, {.name = "pause_menu"}},
    };
    input.progression_catalog = ProgressionCatalogValidationData{
        .level_unlocks = {{.level_id = "level_01"}},
    };
    input.upgrade_catalog = UpgradeCatalogValidationData{
        .base_units = {"operator"},
        .cards = {{.id = "unlock_operator", .effects = {{.unit_id = "operator", .requires_known_unit = true}}}},
    };
    input.unit_catalog = &units;
    LevelDefinition level;
    level.waves.push_back({.wave_number = 1, .spawns = {{.time = 0.0, .type = "jackal"}}});
    input.levels = {{.level_id = "level_01", .definition = level}};
    return input;
}

} // namespace

DEFN_TEST(content_validator_accepts_plain_valid_content) {
    FakeUnitCatalog units;
    const ContentValidationReport report = ContentValidator::validate_loaded_content(make_valid_input(units));

    DEFN_CHECK(report.is_valid());
}

DEFN_TEST(content_validator_reports_plain_cross_reference_errors) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    input.progression_catalog->level_unlocks.push_back({.level_id = "level_02", .requires_completed = "missing"});
    input.upgrade_catalog->base_units.push_back("ghost");
    input.upgrade_catalog->cards[0].prerequisites.push_back("missing_card");
    input.levels[0].definition->waves[0].spawns[0].type = "operator";

    const ContentValidationReport report = ContentValidator::validate_loaded_content(input);

    DEFN_CHECK(!report.is_valid());
    DEFN_CHECK(contains_issue(report, "unknown prerequisite level 'missing'"));
    DEFN_CHECK(contains_issue(report, "base unit 'ghost'"));
    DEFN_CHECK(contains_issue(report, "unknown prerequisite 'missing_card'"));
    DEFN_CHECK(contains_issue(report, "non-hostile spawn type 'operator'"));
}

} // namespace defn
