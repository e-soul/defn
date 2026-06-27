#include "test_harness.h"

#include "content_validator.h"
#include "json_file_loader.h"
#include "level_loader.h"
#include "menu_data_loader.h"
#include "progression_catalog.h"
#include "progression_save_repository.h"
#include "unit_data.h"
#include "upgrade_catalog.h"

#include <algorithm>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>

namespace defn {

namespace {

Array make_array(std::initializer_list<Variant> values) {
    Array array;
    for (const Variant &value : values) {
        array.push_back(value);
    }
    return array;
}

Dictionary make_global_data() {
    Dictionary global_data;

    Dictionary gameplay_rules;
    gameplay_rules["viewport_width"] = 1280;
    gameplay_rules["viewport_height"] = 720;
    gameplay_rules["world_multiplier"] = 3;
    gameplay_rules["belt_top_y"] = 160.0;
    gameplay_rules["belt_bottom_y"] = 420.0;
    gameplay_rules["breach_x"] = 96.0;
    gameplay_rules["spawn_offset"] = 64.0;
    gameplay_rules["friendly_world_margin"] = 128.0;
    global_data["gameplay_rules"] = gameplay_rules;

    Dictionary health_colors;
    health_colors["friendly"] = make_array({0.1, 0.8, 0.1, 1.0});
    health_colors["hostile"] = make_array({0.9, 0.2, 0.2, 1.0});
    global_data["health_bar_color"] = health_colors;

    Dictionary attack_range_variation;
    attack_range_variation["melee"] = make_array({0.9, 1.1});
    attack_range_variation["ranged"] = make_array({0.95, 1.05});
    global_data["attack_range_variation"] = attack_range_variation;

    return global_data;
}

Dictionary make_unit_data() {
    Dictionary units_root;
    Dictionary units;

    Dictionary friendly;
    friendly["side"] = "friendly";
    friendly["hp"] = 120;
    friendly["cost"] = 25;
    friendly["ranged_damage"] = 14;
    friendly["move_speed_pixels_per_second"] = 72.0;
    Dictionary projectile_attack;
    projectile_attack["speed_pixels_per_second"] = 180.0;
    projectile_attack["affected_target_rounding"] = "ceil";
    friendly["projectile_attack"] = projectile_attack;
    units["operator"] = friendly;

    Dictionary hostile;
    hostile["side"] = "hostile";
    hostile["hp"] = 75;
    hostile["bounty"] = 6;
    hostile["ranged_damage"] = 0;
    units["jackal"] = hostile;

    units_root["units"] = units;
    return units_root;
}

bool contains_issue(const ContentValidationReport &report, const String &needle) {
    return std::ranges::any_of(report.issues, [&needle](const String &issue) { return issue.contains(needle); });
}

bool contains_issues(const ContentValidationReport &report, std::initializer_list<String> needles) {
    return std::ranges::all_of(needles, [&report](const String &needle) { return contains_issue(report, needle); });
}

ContentValidationReport make_cross_reference_validation_report() {
    Dictionary level_unlock;
    level_unlock["level_id"] = "level_01";

    Dictionary progression_data;
    progression_data["level_unlocks"] = make_array({level_unlock});
    ProgressionCatalog progression_catalog;
    DEFN_REQUIRE(progression_catalog.load_from_data(progression_data));

    Dictionary bad_effect;
    bad_effect["type"] = "unit_unlock";
    bad_effect["unit_id"] = "ghost_unit";

    Dictionary bad_card;
    bad_card["id"] = "bad_card";
    bad_card["name"] = "Bad Card";
    bad_card["description"] = "Broken";
    bad_card["prerequisites"] = make_array({String("missing_prereq")});
    bad_card["effects"] = make_array({bad_effect});

    Dictionary upgrade_data;
    upgrade_data["base_units"] = make_array({String("ghost_unit")});
    upgrade_data["cards"] = make_array({bad_card});
    UpgradeCatalog upgrade_catalog;
    DEFN_REQUIRE(upgrade_catalog.load_from_data(upgrade_data));

    UnitDataLoader unit_data;
    DEFN_REQUIRE(unit_data.load_from_data(make_unit_data(), make_global_data()));

    MenuContentData menu_data;
    menu_data.menus.push_back({.name = "main_menu"});

    LevelDefinition level_definition;
    level_definition.waves.push_back({.wave_number = 1, .spawns = {{.time = 0.0, .type = "operator"}}});

    return ContentValidator::validate_loaded_content(menu_data, &progression_catalog, &upgrade_catalog, &unit_data,
                                                     {{.level_id = "level_01", .definition = level_definition}});
}

MenuContentData make_required_menu_data() {
    MenuContentData menu_data;
    menu_data.menus.push_back({.name = "main_menu"});
    menu_data.menus.push_back({.name = "game_menu"});
    menu_data.menus.push_back({.name = "options_menu"});
    menu_data.menus.push_back({.name = "pause_menu"});
    return menu_data;
}

ProgressionCatalog make_progression_catalog(std::initializer_list<Dictionary> unlocks) {
    Array unlock_array;
    for (const Dictionary &unlock : unlocks) {
        unlock_array.push_back(unlock);
    }

    Dictionary progression_data;
    progression_data["level_unlocks"] = unlock_array;

    ProgressionCatalog catalog;
    DEFN_REQUIRE(catalog.load_from_data(progression_data));
    return catalog;
}

Dictionary make_level_unlock(const String &level_id, const String &requires_completed = {}) {
    Dictionary unlock;
    unlock["level_id"] = level_id;
    if (!requires_completed.is_empty()) {
        unlock["requires_completed"] = requires_completed;
    }
    return unlock;
}

UpgradeCatalog make_upgrade_catalog(std::initializer_list<Dictionary> cards, std::initializer_list<String> base_units = {String("operator")}) {
    Array base_unit_array;
    for (const String &base_unit : base_units) {
        base_unit_array.push_back(base_unit);
    }

    Array card_array;
    for (const Dictionary &card : cards) {
        card_array.push_back(card);
    }

    Dictionary upgrade_data;
    upgrade_data["base_units"] = base_unit_array;
    upgrade_data["cards"] = card_array;

    UpgradeCatalog catalog;
    DEFN_REQUIRE(catalog.load_from_data(upgrade_data));
    return catalog;
}

Dictionary make_upgrade_card(const String &card_id, const String &effect_unit_id = {}) {
    Dictionary effect;
    effect["type"] = "unit_unlock";
    if (!effect_unit_id.is_empty()) {
        effect["unit_id"] = effect_unit_id;
    }

    Dictionary card;
    card["id"] = card_id;
    card["name"] = String("Card ") + card_id;
    card["description"] = "A test upgrade.";
    card["effects"] = make_array({effect});
    return card;
}

void remove_test_file(const String &path) {
    if (FileAccess::file_exists(path)) {
        DirAccess::remove_absolute(path);
    }
}

void write_text_file(const String &path, const String &text) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    DEFN_REQUIRE(file.is_valid());
    DEFN_REQUIRE(file->store_string(text));
    file->close();
}

} // namespace

DEFN_TEST(upgrade_effect_type_parser_recognizes_known_values) {
    UpgradeEffectType effect_type = UpgradeEffectType::STARTING_ENERGY_DELTA;
    DEFN_CHECK(try_parse_upgrade_effect_type("unit_unlock", effect_type));
    DEFN_CHECK_EQ(effect_type, UpgradeEffectType::UNIT_UNLOCK);
    DEFN_CHECK(!try_parse_upgrade_effect_type("mystery", effect_type));
}

DEFN_TEST(menu_setting_kind_parser_maps_known_and_unknown_values) {
    Dictionary resolution_setting;
    resolution_setting["type"] = "dropdown";
    resolution_setting["setting"] = "resolution";
    DEFN_CHECK_EQ(parse_setting_kind(resolution_setting), MenuSettingKind::RESOLUTION);

    Dictionary unknown_setting;
    unknown_setting["type"] = "slider";
    unknown_setting["setting"] = "bogus";
    DEFN_CHECK_EQ(parse_setting_kind(unknown_setting), MenuSettingKind::UNKNOWN);
}

DEFN_TEST(unit_side_and_rounding_parsers_return_expected_values) {
    Dictionary hostile_unit;
    hostile_unit["side"] = "hostile";
    DEFN_CHECK_EQ(parse_unit_side(hostile_unit), UnitSide::HOSTILE);
    DEFN_CHECK_EQ(parse_splash_target_rounding_mode("floor", SplashTargetRoundingMode::NEAREST), SplashTargetRoundingMode::FLOOR);
    DEFN_CHECK_EQ(parse_splash_target_rounding_mode("weird", SplashTargetRoundingMode::CEIL), SplashTargetRoundingMode::CEIL);
}

DEFN_TEST(level_loader_maps_wave_data_from_dictionary) {
    Dictionary spawn_a;
    spawn_a["time"] = 0.5;
    spawn_a["type"] = "jackal";
    Dictionary spawn_b;
    spawn_b["time"] = 2.0;
    spawn_b["type"] = "operator";

    Dictionary wave;
    wave["wave_number"] = 3;
    wave["spawns"] = make_array({spawn_a, spawn_b});

    Dictionary level_data;
    level_data["level_id"] = 7;
    level_data["name"] = "Factory";
    level_data["starting_core_resource"] = 150;
    level_data["base_integrity"] = 4;
    level_data["background"] = "res://factory.png";
    level_data["waves"] = make_array({wave});

    const auto level = LevelLoader::load_from_data(level_data);
    DEFN_REQUIRE(level.has_value());
    DEFN_CHECK_EQ(level->level_id, 7);
    DEFN_CHECK_EQ(level->waves.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(level->waves[0].spawns.size(), static_cast<size_t>(2));
    DEFN_CHECK_EQ(level->waves[0].spawns[1].type, String("operator"));
}

DEFN_TEST(progression_catalog_loads_unlock_data_from_dictionary) {
    Dictionary unlock_a;
    unlock_a["level_id"] = "level_01";
    unlock_a["rescue_thresholds"] = make_array({100, 200});

    Dictionary unlock_b;
    unlock_b["level_id"] = "level_02";
    unlock_b["requires_completed"] = "level_01";

    Dictionary data;
    data["level_unlocks"] = make_array({unlock_a, unlock_b});

    ProgressionCatalog catalog;
    DEFN_CHECK(catalog.load_from_data(data));
    DEFN_CHECK_EQ(catalog.get_level_unlocks().size(), static_cast<size_t>(2));
    DEFN_CHECK_EQ(catalog.get_level_unlocks()[0].rescue_thresholds[1], 200);
}

DEFN_TEST(upgrade_catalog_loads_cards_from_dictionary) {
    Dictionary effect;
    effect["type"] = "unit_unlock";
    effect["unit_id"] = "operator";

    Dictionary card;
    card["id"] = "unlock_operator";
    card["name"] = "Unlock Operator";
    card["description"] = "Deploy the operator.";
    card["effects"] = make_array({effect});
    card["prerequisites"] = make_array({String("base_card")});

    Dictionary data;
    data["draft_size"] = 4;
    data["reserve_unit_slot"] = false;
    data["base_units"] = make_array({String("operator")});
    data["cards"] = make_array({card});

    UpgradeCatalog catalog;
    DEFN_CHECK(catalog.load_from_data(data));
    DEFN_CHECK_EQ(catalog.get_draft_size(), 4);
    DEFN_CHECK(!catalog.should_reserve_unit_slot());
    DEFN_CHECK_EQ(catalog.get_cards().size(), static_cast<size_t>(1));
    DEFN_CHECK(catalog.get_cards()[0].grants_unit_unlock());
}

DEFN_TEST(unit_data_loader_loads_globals_and_units_from_dictionaries) {
    UnitDataLoader loader;
    DEFN_CHECK(loader.load_from_data(make_unit_data(), make_global_data()));

    const auto friendly = loader.get_unit("operator");
    DEFN_REQUIRE(friendly.has_value());
    DEFN_REQUIRE(friendly->projectile_attack.has_value());
    DEFN_CHECK_EQ(friendly->side, UnitSide::FRIENDLY);
    DEFN_CHECK_EQ(friendly->projectile_attack->affected_target_rounding, SplashTargetRoundingMode::CEIL);
    DEFN_CHECK_EQ(loader.get_globals().gameplay_rules.viewport_width, static_cast<real_t>(1280.0F));
    DEFN_CHECK_EQ(loader.get_friendly_units().size(), static_cast<size_t>(1));
}

DEFN_TEST(json_file_loader_reads_dictionary_and_rejects_bad_inputs) {
    const String valid_path = "user://defn_json_loader_valid_test.json";
    const String invalid_path = "user://defn_json_loader_invalid_test.json";
    const String missing_path = "user://defn_json_loader_missing_test.json";
    remove_test_file(valid_path);
    remove_test_file(invalid_path);
    remove_test_file(missing_path);

    write_text_file(valid_path, R"({"answer": 42, "label": "ok"})");
    const auto loaded = JsonFileLoader::load_dictionary(valid_path, "JsonFileLoaderTest");
    DEFN_REQUIRE(loaded.has_value());
    DEFN_CHECK_EQ(static_cast<int>((*loaded)["answer"]), 42);
    DEFN_CHECK_EQ(String((*loaded)["label"]), String("ok"));

    write_text_file(invalid_path, "{");
    DEFN_CHECK(!JsonFileLoader::load_dictionary(invalid_path, "JsonFileLoaderTest").has_value());
    DEFN_CHECK(!JsonFileLoader::load_dictionary(missing_path, "JsonFileLoaderTest").has_value());

    remove_test_file(valid_path);
    remove_test_file(invalid_path);
}

DEFN_TEST(progression_save_repository_round_trips_player_profile) {
    const String path = "user://defn_progression_save_roundtrip_test.json";
    remove_test_file(path);

    PlayerProfile profile;
    profile.total_score = 900;
    profile.completed_levels.insert("level_01");
    profile.completed_levels.insert("level_02");
    profile.best_level_scores["level_01"] = 350;
    profile.owned_upgrade_counts["quick_reload"] = 2;
    profile.claimed_level_upgrades["level_01"] = "quick_reload";
    profile.claimed_rescue_drafts["level_02"] = 1;

    DEFN_REQUIRE(ProgressionSaveRepository::save(path, profile));
    const auto loaded = ProgressionSaveRepository::load(path);
    DEFN_REQUIRE(loaded.has_value());
    DEFN_CHECK_EQ(loaded->total_score, 900);
    DEFN_CHECK_EQ(loaded->completed_levels.count("level_01"), static_cast<size_t>(1));
    DEFN_CHECK_EQ(loaded->completed_levels.count("level_02"), static_cast<size_t>(1));
    DEFN_CHECK_EQ(loaded->best_level_scores.at("level_01"), 350);
    DEFN_CHECK_EQ(loaded->owned_upgrade_counts.at("quick_reload"), 2);
    DEFN_CHECK_EQ(loaded->claimed_level_upgrades.at("level_01"), String("quick_reload"));
    DEFN_CHECK_EQ(loaded->claimed_rescue_drafts.at("level_02"), 1);

    remove_test_file(path);
}

DEFN_TEST(progression_save_repository_handles_missing_invalid_and_clamped_counts) {
    const String invalid_path = "user://defn_progression_save_invalid_test.json";
    const String clamped_path = "user://defn_progression_save_clamped_test.json";
    const String missing_path = "user://defn_progression_save_missing_test.json";
    remove_test_file(invalid_path);
    remove_test_file(clamped_path);
    remove_test_file(missing_path);

    DEFN_CHECK(!ProgressionSaveRepository::load(missing_path).has_value());

    write_text_file(invalid_path, "{");
    DEFN_CHECK(!ProgressionSaveRepository::load(invalid_path).has_value());

    write_text_file(clamped_path, R"({"total_score": 12, "owned_upgrade_counts": {"bad_debt": -3}, "rescue_drafts_claimed": {"level_02": -2}})");
    const auto loaded = ProgressionSaveRepository::load(clamped_path);
    DEFN_REQUIRE(loaded.has_value());
    DEFN_CHECK_EQ(loaded->total_score, 12);
    DEFN_CHECK_EQ(loaded->owned_upgrade_counts.at("bad_debt"), 0);
    DEFN_CHECK_EQ(loaded->claimed_rescue_drafts.at("level_02"), 0);

    remove_test_file(invalid_path);
    remove_test_file(clamped_path);
}

DEFN_TEST(content_validator_accepts_valid_loaded_content) {
    const ProgressionCatalog progression_catalog = make_progression_catalog({make_level_unlock("level_01")});
    const UpgradeCatalog upgrade_catalog = make_upgrade_catalog({make_upgrade_card("unlock_operator", "operator")});

    UnitDataLoader unit_data;
    DEFN_REQUIRE(unit_data.load_from_data(make_unit_data(), make_global_data()));

    LevelDefinition level_definition;
    level_definition.waves.push_back({.wave_number = 1, .spawns = {{.time = 0.0, .type = "jackal"}}});

    const ContentValidationReport report = ContentValidator::validate_loaded_content(make_required_menu_data(), &progression_catalog, &upgrade_catalog,
                                                                                     &unit_data, {{.level_id = "level_01", .definition = level_definition}});
    DEFN_CHECK(report.is_valid());
}

DEFN_TEST(content_validator_reports_catalog_and_level_shape_issues) {
    MenuContentData menu_data = make_required_menu_data();
    menu_data.menus[2].settings.push_back({.setting_id = "mystery", .kind = MenuSettingKind::UNKNOWN});

    const ProgressionCatalog progression_catalog =
        make_progression_catalog({make_level_unlock(""), make_level_unlock("level_01"), make_level_unlock("level_01"),
                                  make_level_unlock("level_02", "missing_level"), make_level_unlock("level_03")});
    const UpgradeCatalog upgrade_catalog = make_upgrade_catalog({make_upgrade_card("duplicate"), make_upgrade_card("duplicate")});

    UnitDataLoader unit_data;
    DEFN_REQUIRE(unit_data.load_from_data(make_unit_data(), make_global_data()));

    LevelDefinition empty_level;
    LevelDefinition unknown_spawn_level;
    unknown_spawn_level.waves.push_back({.wave_number = 1, .spawns = {{.time = 0.0, .type = "ghost"}}});

    const ContentValidationReport report = ContentValidator::validate_loaded_content(
        menu_data, &progression_catalog, &upgrade_catalog, &unit_data,
        {{.level_id = "level_01", .definition = empty_level}, {.level_id = "level_02", .definition = unknown_spawn_level}, {.level_id = "level_03"}});

    DEFN_CHECK(!report.is_valid());
    DEFN_CHECK(contains_issues(report, {"unsupported setting 'mystery'", "empty level_id", "duplicate level_id 'level_01'",
                                        "requires unknown prerequisite level 'missing_level'", "duplicate upgrade id 'duplicate'",
                                        "level 'level_01' has no waves", "unknown spawn type 'ghost'", "missing or invalid level definition for 'level_03'"}));
}

DEFN_TEST(content_validator_reports_cross_reference_issues_from_loaded_data) {
    const ContentValidationReport report = make_cross_reference_validation_report();

    DEFN_CHECK(!report.is_valid());
    DEFN_CHECK(contains_issues(report, {"missing required menu", "base unit 'ghost_unit'", "unknown prerequisite 'missing_prereq'", "unknown unit 'ghost_unit'",
                                        "non-hostile spawn type 'operator'"}));
}

} // namespace defn