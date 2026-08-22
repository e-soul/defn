// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

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

UiThemeData make_valid_ui_theme() {
    UiThemeData theme;
    theme.surfaces["panel"] = {.bg_role = "surface", .border_role = "border", .shape_role = "corner_md", .content_margin_role = "md"};
    theme.text_styles["screen_title"] = {.font_size_role = "title", .color_role = "text_primary"};
    theme.text_styles["secondary"] = {.font_size_role = "body", .color_role = "text_secondary"};
    theme.buttons["menu"] = {.font_size_role = "body", .shape_role = "corner_md", .normal = {.bg_role = "surface", .font_role = "text_primary"}};
    theme.medallions["available"] = {.mark = "res://mark.svg", .color_role = "accent"};
    theme.medallions["completed"] = {.mark = "res://mark.svg", .color_role = "text_primary"};
    theme.medallions["frontier"] = {.mark = "res://mark.svg", .color_role = "accent"};
    theme.medallions["locked"] = {.mark = "res://mark.svg", .color_role = "text_secondary"};
    theme.screen = {.backdrop_role = "overlay_scrim",
                    .title_text_style = "screen_title",
                    .subtitle_text_style = "secondary",
                    .panel_surface = "panel",
                    .footer_gap_role = "md"};
    return theme;
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

DEFN_TEST(content_validator_rejects_invalid_field_promotion_rules) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    input.field_promotion_rules = {
        .damage_threshold = 0,
        .damage_multiplier = 0.9,
        .attack_period_multiplier = 1.1,
        .health_multiplier = 0.9,
    };

    const ContentValidationReport report = ContentValidator::validate_loaded_content(input);
    DEFN_CHECK(contains_issue(report, "damage_threshold"));
    DEFN_CHECK(contains_issue(report, "damage_multiplier"));
    DEFN_CHECK(contains_issue(report, "attack_period_multiplier"));
    DEFN_CHECK(contains_issue(report, "health_multiplier"));
}

DEFN_TEST(content_validator_rejects_non_normalized_level_base_position) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    input.levels = {{.level_id = "level_01",
                     .definition = LevelDefinition{
                         .base_position_ratio = {.x = 1.1F, .y = 0.5F},
                         .waves = {{.wave_number = 1, .spawns = {{.time = 0.0, .type = "jackal"}}}},
                     }}};

    const ContentValidationReport report = ContentValidator::validate_loaded_content(input);

    DEFN_CHECK(contains_issue(report, "base_position must be normalized"));
}

DEFN_TEST(content_validator_rejects_invalid_level_belt_width) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    input.levels[0].definition->belt_width_ratio = {.x = 0.75F, .y = 0.75F};

    ContentValidationReport report = ContentValidator::validate_loaded_content(input);
    DEFN_CHECK(contains_issue(report, "belt_width must contain two distinct normalized ratios"));

    input.levels[0].definition->belt_width_ratio = {.x = -0.1F, .y = 0.75F};
    report = ContentValidator::validate_loaded_content(input);
    DEFN_CHECK(contains_issue(report, "belt_width must contain two distinct normalized ratios"));
}

DEFN_TEST(content_validator_accepts_campaign_map_matching_progression) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    input.campaign_map = CampaignMapDefinition{
        .background = {.path = "res://map.jpg"},
        .missions = {{.level_id = "level_01",
                      .position_normalized = {.x = 0.2F, .y = 0.3F},
                      .threat_id = "low",
                      .ambience = CampaignMapAmbience::DUST,
                      .preview = {.texture = {.path = "res://preview.jpg"}, .focus_x = 0.5F, .focus_y = 0.5F, .node_zoom = 1.0F, .dossier_zoom = 1.0F}}},
    };

    DEFN_CHECK(ContentValidator::validate_loaded_content(input).is_valid());
}

DEFN_TEST(content_validator_reports_campaign_map_shape_cross_references_and_assets) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    input.progression_catalog->level_unlocks.push_back({.level_id = "level_02", .requires_completed = "level_01"});
    input.campaign_map = CampaignMapDefinition{
        .background = {},
        .missions = {{.level_id = "ghost",
                      .position_normalized = {.x = -0.1F, .y = 0.3F},
                      .threat_id = "impossible",
                      .ambience = CampaignMapAmbience::UNKNOWN,
                      .preview = {.texture = {.path = "res://preview.jpg"}, .focus_x = 1.2F, .node_zoom = 0.9F}},
                     {.level_id = "ghost", .position_normalized = {.x = 0.2F, .y = 0.3F}}},
    };
    input.missing_campaign_assets = {"res://missing.jpg"};

    const ContentValidationReport report = ContentValidator::validate_loaded_content(input);
    DEFN_CHECK(contains_issue(report, "background path is empty"));
    DEFN_CHECK(contains_issue(report, "preview for 'ghost' path is empty"));
    DEFN_CHECK(contains_issue(report, "focus must be normalized"));
    DEFN_CHECK(contains_issue(report, "zoom values"));
    DEFN_CHECK(contains_issue(report, "duplicate level_id 'ghost'"));
    DEFN_CHECK(contains_issue(report, "unknown threat"));
    DEFN_CHECK(contains_issue(report, "unknown ambience"));
    DEFN_CHECK(contains_issue(report, "missing progression level 'level_01'"));
    DEFN_CHECK(contains_issue(report, "unknown level 'ghost'"));
    DEFN_CHECK(contains_issue(report, "resource does not exist"));
}

DEFN_TEST(content_validator_accepts_a_ui_theme_with_resolvable_roles) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    input.ui_theme = make_valid_ui_theme();

    const ContentValidationReport report = ContentValidator::validate_loaded_content(input);
    DEFN_CHECK(report.is_valid());
}

DEFN_TEST(content_validator_reports_medallion_gaps_and_missing_marks) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    UiThemeData theme = make_valid_ui_theme();
    theme.medallions.erase("frontier");
    theme.medallions["locked"] = {.mark = "", .color_role = "ghost_medallion_color"};
    input.ui_theme = theme;
    input.missing_ui_theme_assets = {"res://assets/ui/medallions/gone.svg"};

    const ContentValidationReport report = ContentValidator::validate_loaded_content(input);
    DEFN_CHECK(!report.is_valid());
    DEFN_CHECK(contains_issue(report, "missing required medallion 'frontier'"));
    DEFN_CHECK(contains_issue(report, "medallion 'locked' is missing a mark"));
    DEFN_CHECK(contains_issue(report, "unknown color role 'ghost_medallion_color'"));
    DEFN_CHECK(contains_issue(report, "ui_theme.json resource does not exist: 'res://assets/ui/medallions/gone.svg'"));
}

DEFN_TEST(content_validator_reports_unknown_ui_theme_roles) {
    FakeUnitCatalog units;
    ContentValidationInput input = make_valid_input(units);
    UiThemeData theme;
    theme.surfaces["panel"] = {.bg_role = "ghost_color", .shape_role = "ghost_shape", .content_margin_role = "ghost_spacing"};
    theme.text_styles["screen_title"] = {.font_size_role = "ghost_size", .color_role = "text_primary"};
    theme.buttons["menu"] = {.normal = {.bg_role = "ghost_button_color"}};
    theme.screen = {.title_text_style = "missing_style", .subtitle_text_style = "screen_title", .panel_surface = "missing_surface"};
    input.ui_theme = theme;

    const ContentValidationReport report = ContentValidator::validate_loaded_content(input);
    DEFN_CHECK(!report.is_valid());
    DEFN_CHECK(contains_issue(report, "unknown color role 'ghost_color'"));
    DEFN_CHECK(contains_issue(report, "unknown shape role 'ghost_shape'"));
    DEFN_CHECK(contains_issue(report, "unknown spacing role 'ghost_spacing'"));
    DEFN_CHECK(contains_issue(report, "unknown font size role 'ghost_size'"));
    DEFN_CHECK(contains_issue(report, "unknown color role 'ghost_button_color'"));
    DEFN_CHECK(contains_issue(report, "unknown text style 'missing_style'"));
    DEFN_CHECK(contains_issue(report, "unknown surface 'missing_surface'"));
}

} // namespace defn
