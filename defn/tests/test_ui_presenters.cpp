#include "test_harness.h"

#include "attack_target_resolver.h"
#include "base_objective.h"
#include "base_objective_factory.h"
#include "camera_scroll_controller.h"
#include "deploy_card_presenter.h"
#include "game_background_builder.h"
#include "grid_manager.h"
#include "hud.h"
#include "match_result_cutscene_view_model.h"
#include "menu_manager.h"
#include "pause_menu.h"
#include "progression_stats_screen_view.h"
#include "projectile_attack.h"
#include "projectile_factory.h"
#include "score_screen_view.h"
#include "unit.h"
#include "unit_factory.h"
#include "upgrade_card_presenter.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/parallax2d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

namespace defn {

namespace {

template <typename NodeType> void collect_nodes(Node *root, std::vector<NodeType *> &result) {
    if (root == nullptr) {
        return;
    }

    if (auto *typed_node = Object::cast_to<NodeType>(root); typed_node != nullptr) {
        result.push_back(typed_node);
    }

    const int child_count = root->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index) {
        collect_nodes(root->get_child(child_index), result);
    }
}

std::vector<Label *> collect_labels(Node *root) {
    std::vector<Label *> labels;
    collect_nodes(root, labels);
    return labels;
}

std::vector<Button *> collect_buttons(Node *root) {
    std::vector<Button *> buttons;
    collect_nodes(root, buttons);
    return buttons;
}

bool has_label_text(Node *root, const String &text) {
    const std::vector<Label *> labels = collect_labels(root);
    return std::ranges::any_of(labels, [&text](const Label *label) { return label->get_text() == text; });
}

Label *find_label_by_text(Node *root, const String &text) {
    const std::vector<Label *> labels = collect_labels(root);
    const auto iter = std::ranges::find_if(labels, [&text](const Label *label) { return label->get_text() == text; });
    return iter == labels.end() ? nullptr : *iter;
}

bool has_label_containing(Node *root, const String &needle) {
    const std::vector<Label *> labels = collect_labels(root);
    return std::ranges::any_of(labels, [&needle](const Label *label) { return label->get_text().contains(needle); });
}

Button *find_button_by_text(Node *root, const String &text) {
    for (auto *button : collect_buttons(root)) {
        if (button->get_text() == text) {
            return button;
        }
    }

    return nullptr;
}

Callable make_valid_callable(Object *receiver) { return {receiver, "queue_free"}; }

UnitConfig make_presenter_unit_config(const std::string &name, int cost) {
    UnitConfig config;
    config.name = name;
    config.cost = cost;
    config.hp = 120;
    config.melee_damage = 0;
    config.ranged_damage = 0;
    config.move_speed_pixels_per_second = 0.0F;
    config.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.animations.push_back(
        {"walk", {.path_template = "res://assets/Spec_Ops_-_Game_Sprites/png/Soldier4/Climb__%03d.png", .frame_count = 1, .loop = true}});
    return config;
}

bool has_node_named(Node *root, const String &name) {
    if (root == nullptr) {
        return false;
    }
    if (String(root->get_name()) == name) {
        return true;
    }

    const int child_count = root->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index) {
        if (has_node_named(root->get_child(child_index), name)) {
            return true;
        }
    }

    return false;
}

bool has_all_buttons(Node *root, std::initializer_list<const char *> labels) {
    return std::ranges::all_of(labels, [root](const char *label) { return find_button_by_text(root, String(label)) != nullptr; });
}

bool has_all_labels(Node *root, std::initializer_list<const char *> labels) {
    return std::ranges::all_of(labels, [root](const char *label) { return has_label_text(root, String(label)); });
}

bool has_all_named_nodes(Node *root, std::initializer_list<const char *> names) {
    return std::ranges::all_of(names, [root](const char *name) { return has_node_named(root, String(name)); });
}

bool nearly_equal(double left, double right) { return std::abs(left - right) <= 0.001; }

bool color_matches(const godot::Color &actual, const godot::Color &expected) {
    return nearly_equal(actual.r, expected.r) && nearly_equal(actual.g, expected.g) && nearly_equal(actual.b, expected.b) && nearly_equal(actual.a, expected.a);
}

bool label_font_color_matches(Node *root, const String &text, const godot::Color &expected_color) {
    Label *label = find_label_by_text(root, text);
    return label != nullptr && label->has_theme_color_override("font_color") && color_matches(label->get_theme_color("font_color"), expected_color);
}

bool button_minimum_size_is(Button *button, double width, double height) {
    if (button == nullptr) {
        return false;
    }

    const godot::Vector2 minimum_size = button->get_custom_minimum_size();
    return nearly_equal(minimum_size.x, width) && nearly_equal(minimum_size.y, height);
}

bool selected_upgrade_button_matches(Button *button) {
    return button != nullptr && button->is_disabled() && button_minimum_size_is(button, 180.0, 220.0) && button->has_theme_stylebox_override("normal") &&
           button->has_theme_stylebox_override("disabled") && has_all_labels(button, {"+", "x2", "Rapid Reload", "Shoot more often."});
}

bool fallback_upgrade_button_matches(Button *button) { return button != nullptr && !button->is_disabled() && has_all_labels(button, {"?", "Upgrade"}); }

bool progression_view_has_initial_entity_state(ProgressionStatsScreenView *view) {
    Button *selected_button = find_button_by_text(view, "Breacher");
    Button *locked_button = find_button_by_text(view, "Marksman [Locked]");
    return has_label_text(view, "Breacher") && view->find_child("EntityPortraitFallback", true, false) != nullptr && selected_button != nullptr &&
           selected_button->has_theme_stylebox_override("normal") && selected_button->has_theme_stylebox_override("hover") &&
           selected_button->get_theme_stylebox("normal") == selected_button->get_theme_stylebox("hover") && locked_button != nullptr &&
           locked_button->is_disabled();
}

bool score_screen_view_matches_victory_layout(Node *parent, const ScoreScreenViewNodes &view) {
    return view.overlay != nullptr && view.panel != nullptr && parent->get_child_count() == 1 && nearly_equal(view.overlay->get_color().a, 0.7);
}

bool score_screen_has_victory_content(Node *overlay) {
    return has_all_labels(overlay, {"VICTORY", "FIRST CLEAR UPGRADE: Level 01", "Level 01 cleared for the first time.", "NEW UNLOCK: Level 02!",
                                    "YOUR UPGRADES", "Rapid Reload", "Owned Upgrade"});
}

bool score_screen_has_disabled_primary_actions(Node *overlay) {
    Button *next_button = find_button_by_text(overlay, "Next Level");
    Button *retry_button = find_button_by_text(overlay, "Retry");
    Button *menu_button = find_button_by_text(overlay, "Main Menu");
    return next_button != nullptr && retry_button != nullptr && menu_button != nullptr && next_button->is_disabled() && retry_button->is_disabled() &&
           menu_button->is_disabled();
}

bool hud_has_initial_state(HUD *hud) { return has_label_containing(hud, "Energy: 100") && has_all_labels(hud, {"Score: 0", "WAVE 1 / 3"}); }

bool hud_has_updated_match_state(HUD *hud) {
    return has_label_containing(hud, "Energy: 42") && has_all_labels(hud, {"Score: 125", "WAVE 2 / 5", "LEVEL 3 - Factory"});
}

Button *find_deploy_card_button(Node *root) {
    for (auto *button : collect_buttons(root)) {
        if (button->get_text().is_empty()) {
            return button;
        }
    }

    return nullptr;
}

bool hud_has_hearts_and_operator_card(HUD *hud) { return collect_labels(hud).size() >= static_cast<size_t>(5) && has_label_text(hud, "Operator"); }

bool hud_deploy_card_disabled_for_resource(HUD *hud, int core_resource, bool expected_disabled) {
    hud->update_card_affordability(core_resource);
    Button *deploy_button = find_deploy_card_button(hud);
    return deploy_button != nullptr && deploy_button->is_disabled() == expected_disabled;
}

bool unit_has_passive_factory_stack(Unit *unit) {
    return has_all_named_nodes(unit, {"HealthComponent", "HealthBarWidget", "AnimationController"}) && !has_node_named(unit, "MovementComponent") &&
           !has_node_named(unit, "CombatComponent");
}

bool unit_has_combat_factory_stack(Unit *unit) { return has_all_named_nodes(unit, {"DetectionComponent", "MovementComponent", "CombatComponent"}); }

bool menu_manager_shows_main_menu(MenuManager *menu_manager) {
    return has_label_containing(menu_manager, "Career Score:") && has_all_buttons(menu_manager, {"Play", "Options", "Quit"});
}

bool menu_manager_shows_game_menu(MenuManager *menu_manager) { return has_all_buttons(menu_manager, {"New/Continue Game", "Progress", "Main Menu"}); }

bool menu_manager_shows_options_menu(MenuManager *menu_manager) {
    return has_all_labels(menu_manager, {"Video", "Display Mode", "Resolution", "VSync", "Audio", "Master Volume"}) && has_all_buttons(menu_manager, {"Back"});
}

bool menu_manager_shows_level_select(MenuManager *menu_manager) {
    return has_all_labels(menu_manager, {"SELECT LEVEL"}) && has_all_buttons(menu_manager, {"Back"});
}

bool menu_manager_shows_progression(MenuManager *menu_manager) {
    return has_all_labels(menu_manager, {"COMMAND ROSTER"}) && has_all_buttons(menu_manager, {"All Owned Upgrades", "Back"});
}

bool menu_manager_background_covers_viewport(MenuManager *menu_manager) {
    std::vector<TextureRect *> texture_rects;
    collect_nodes(menu_manager, texture_rects);
    if (texture_rects.empty()) {
        return false;
    }

    TextureRect *background = texture_rects.front();
    return background->get_stretch_mode() == TextureRect::STRETCH_KEEP_ASPECT_COVERED && background->get_expand_mode() == TextureRect::EXPAND_IGNORE_SIZE;
}

bool base_objective_has_basic_stack(BaseObjective *objective) {
    return !objective->is_dead() && objective->get_hitbox() != nullptr &&
           has_all_named_nodes(objective, {"TargetAnchor", "HealthComponent", "HitboxComponent"}) && !has_node_named(objective, "CombatComponent");
}

bool base_objective_has_attack_stack(BaseObjective *objective) {
    return objective->get_side() == UnitSide::FRIENDLY &&
           has_all_named_nodes(objective, {"TowerSprite", "AnimationController", "DetectionComponent", "CombatComponent"});
}

bool pause_menu_has_expected_buttons(PauseMenu *pause_menu) { return has_all_buttons(pause_menu, {"Resume", "Main Menu"}); }

bool pause_menu_overlay_visible(PauseMenu *pause_menu, bool expected_visible) {
    std::vector<ColorRect *> overlays;
    collect_nodes(pause_menu, overlays);
    return !overlays.empty() && overlays.front()->is_visible() == expected_visible;
}

GameplayRules make_camera_test_rules() {
    GameplayRules rules;
    rules.viewport_width = 1000.0F;
    rules.viewport_height = 600.0F;
    rules.world_multiplier = 4;
    rules.belt_top_y = 100.0F;
    rules.belt_bottom_y = 300.0F;
    rules.scroll_trigger_extra_height = 80.0F;
    rules.camera_scroll_step_factor = 0.25F;
    return rules;
}

bool camera_scroll_controller_has_initial_positions(const CameraScrollController &controller) {
    return nearly_equal(controller.get_world_width(), 3000.0) && nearly_equal(controller.calculate_world_width(700.0F), 2800.0) &&
           nearly_equal(controller.get_trigger_height(), 280.0) && nearly_equal(controller.get_camera_anchor_position().x, 500.0) &&
           nearly_equal(controller.get_camera_anchor_position().y, 300.0) && nearly_equal(controller.get_left_trigger_position().x, 250.0) &&
           nearly_equal(controller.get_left_trigger_position().y, 200.0) && nearly_equal(controller.get_right_trigger_position().x, 750.0) &&
           nearly_equal(controller.get_right_trigger_position().y, 200.0);
}

bool background_build_matches_rules(const GameBackgroundBuildResult &result, const GameplayRules &rules) {
    if (result.background == nullptr || result.background->get_child_count() != 1) {
        return false;
    }

    const auto *sprite = Object::cast_to<Sprite2D>(result.background->get_child(0));
    if (sprite == nullptr || !sprite->get_texture().is_valid()) {
        return false;
    }

    const godot::Vector2 texture_size = sprite->get_texture()->get_size();
    const real_t expected_scale = rules.viewport_height / texture_size.y;
    const real_t expected_width = texture_size.x * expected_scale * static_cast<real_t>(rules.world_multiplier);
    return String(result.background->get_name()) == "Background" && nearly_equal(result.world_width, expected_width) &&
           nearly_equal(result.background->get_repeat_size().x, texture_size.x * expected_scale) &&
           result.background->get_repeat_times() == rules.world_multiplier && nearly_equal(sprite->get_scale().x, expected_scale) &&
           nearly_equal(sprite->get_scale().y, expected_scale) && !sprite->is_centered();
}

UnitConfig make_objective_visual_config(UnitSide side) {
    UnitConfig config = make_presenter_unit_config("objective", 0);
    config.side = side;
    config.animations.clear();
    return config;
}

BaseObjective *add_test_objective(Node *parent, UnitSide side, int max_hp, const godot::Vector2 &target_position) {
    auto *objective = BaseObjectiveFactory::create(max_hp, target_position, make_objective_visual_config(side));
    parent->add_child(objective);
    return objective;
}

} // namespace

DEFN_TEST(deploy_card_presenter_builds_card_content_from_unit_config) {
    auto *receiver = memnew(Node);

    UnitConfig config;
    config.name = "operator";
    config.cost = 25;
    config.animations.push_back({"shoot", {.path_template = "res://assets/tower.png"}});

    auto *button = DeployCardPresenter::create(config, make_valid_callable(receiver));

    DEFN_REQUIRE(button != nullptr);
    DEFN_CHECK_CLOSE(button->get_custom_minimum_size().x, 190.0, 0.001);
    DEFN_CHECK_CLOSE(button->get_custom_minimum_size().y, 110.0, 0.001);
    DEFN_CHECK(button->has_theme_stylebox_override("normal"));
    DEFN_CHECK(button->has_theme_stylebox_override("hover"));
    DEFN_CHECK(button->has_theme_stylebox_override("pressed"));
    DEFN_CHECK(button->has_theme_stylebox_override("disabled"));
    DEFN_CHECK(has_label_text(button, "Operator"));
    DEFN_CHECK(has_label_containing(button, "25"));

    memdelete(button);
    memdelete(receiver);
}

DEFN_TEST(upgrade_card_presenter_builds_selected_disabled_and_fallback_cards) {
    auto *receiver = memnew(Node);

    UpgradeCardViewModel upgrade;
    upgrade.id = "rapid_reload";
    upgrade.name = "Rapid Reload";
    upgrade.description = "Shoot more often.";
    upgrade.emoji = "+";
    upgrade.owned_count = 2;

    auto *selected_button = UpgradeCardPresenter::create(upgrade, true, true, make_valid_callable(receiver));
    DEFN_REQUIRE(selected_button != nullptr);
    DEFN_CHECK(selected_upgrade_button_matches(selected_button));

    UpgradeCardViewModel fallback;
    fallback.description = "Fallback description.";
    auto *fallback_button = UpgradeCardPresenter::create(fallback, false, false, {});
    DEFN_REQUIRE(fallback_button != nullptr);
    DEFN_CHECK(fallback_upgrade_button_matches(fallback_button));

    memdelete(selected_button);
    memdelete(fallback_button);
    memdelete(receiver);
}

DEFN_TEST(score_screen_presenter_builds_victory_screen_with_rewards_and_disabled_actions) {
    auto *parent = memnew(Node);

    ScoreScreenModel model;
    model.victory = true;
    model.enemies_killed = 4;
    model.kill_score = 80;
    model.hearts_remaining = 2;
    model.hearts_total = 3;
    model.integrity_bonus = 50;
    model.completion_bonus = 100;
    model.level_score = 230;
    model.new_total_score = 900;
    model.next_level_id = "level_02";
    model.new_unlocks.emplace_back("NEW UNLOCK: Level 02!");
    model.reward.title = "FIRST CLEAR UPGRADE: Level 01";
    model.reward.subtitle = "Level 01 cleared for the first time.";
    model.reward.available_upgrades.push_back({.id = "rapid_reload", .name = "Rapid Reload", .description = "Shoot more often.", .emoji = "+"});
    model.owned_upgrades.push_back({.id = "owned", .name = "Owned Upgrade", .description = "Already claimed.", .emoji = "*", .owned_count = 1});

    const Callable action = make_valid_callable(parent);
    const ScoreScreenViewNodes view =
        ScoreScreenView::show(parent, model, {.on_next_level = action, .on_retry = action, .on_main_menu = action, .on_select_upgrade = action});

    DEFN_CHECK(score_screen_view_matches_victory_layout(parent, view));
    DEFN_CHECK(score_screen_has_victory_content(view.overlay));
    DEFN_CHECK(score_screen_has_disabled_primary_actions(view.overlay));

    memdelete(parent);
}

DEFN_TEST(score_screen_presenter_handles_null_parent_and_defeat_without_next_level) {
    DEFN_CHECK(ScoreScreenView::show(nullptr, {}, {}).overlay == nullptr);

    auto *parent = memnew(Node);
    ScoreScreenModel model;
    model.victory = false;
    model.enemies_killed = 1;
    model.kill_score = 10;
    model.hearts_remaining = 0;
    model.hearts_total = 3;
    model.level_score = 10;
    model.new_total_score = 20;

    const ScoreScreenViewNodes view = ScoreScreenView::show(parent, model, {});

    DEFN_REQUIRE(view.overlay != nullptr);
    DEFN_CHECK(has_label_text(view.overlay, "DEFEAT"));
    DEFN_CHECK(find_button_by_text(view.overlay, "Next Level") == nullptr);
    DEFN_REQUIRE(find_button_by_text(view.overlay, "Retry") != nullptr);
    DEFN_REQUIRE(find_button_by_text(view.overlay, "Main Menu") != nullptr);

    memdelete(parent);
}

DEFN_TEST(hud_builds_and_updates_core_labels_cards_and_score_screen) {
    auto *hud = memnew(HUD);
    hud->_ready();

    DEFN_CHECK(hud_has_initial_state(hud));

    hud->update_core_resource(42);
    hud->update_score(125);
    hud->update_wave(2, 5);
    hud->set_level(3, "Factory");
    DEFN_CHECK(hud_has_updated_match_state(hud));

    hud->set_level(4, {});
    DEFN_CHECK(has_label_text(hud, "LEVEL 4"));
    hud->set_level(0, "Challenge");
    DEFN_CHECK(has_label_text(hud, "Challenge"));

    hud->update_hearts(5);
    UnitConfig operator_config = make_presenter_unit_config("operator", 25);
    hud->set_friendly_units({operator_config});
    DEFN_CHECK(hud_has_hearts_and_operator_card(hud));

    DEFN_CHECK(hud_deploy_card_disabled_for_resource(hud, 10, true));
    DEFN_CHECK(hud_deploy_card_disabled_for_resource(hud, 30, false));

    ScoreScreenModel summary;
    summary.victory = false;
    summary.current_level_id = "level_01";
    summary.hearts_total = 3;
    hud->show_score_screen(summary);
    DEFN_CHECK(has_label_text(hud, "DEFEAT"));

    memdelete(hud);
}

DEFN_TEST(hud_shows_and_hides_match_result_banner) {
    auto *hud = memnew(HUD);
    hud->_ready();

    hud->show_match_result_banner(MatchResultCutscenePresenter::build(true));
    DEFN_CHECK(has_label_text(hud, "AREA SECURED"));
    DEFN_CHECK(label_font_color_matches(hud, "AREA SECURED", godot::Color(0.2, 1.0, 0.3, 1.0)));
    hud->hide_match_result_banner();
    DEFN_CHECK(!has_label_text(hud, "AREA SECURED"));

    hud->show_match_result_banner(MatchResultCutscenePresenter::build(false));
    DEFN_CHECK(has_label_text(hud, "DEFEAT"));
    DEFN_CHECK(label_font_color_matches(hud, "DEFEAT", godot::Color(1.0, 0.2, 0.2, 1.0)));

    memdelete(hud);
}

DEFN_TEST(unit_factory_creates_materializes_and_initializes_runtime_profiles) {
    UnitFactory::initialize(nullptr);

    UnitConfig passive_config = make_presenter_unit_config("operator", 25);
    passive_config.health_bar_color = {.r = 0.1F, .g = 0.8F, .b = 0.1F, .a = 1.0F};
    passive_config.health_bar_offset = {.x = 0.0F, .y = -20.0F};

    UnitRuntimeProfile passive_profile = UnitRuntimeProfile::passive_static();
    const ResolvedUnitRuntimeConfig passive_resolved_config{
        .melee_attack_range = passive_config.melee_attack_range,
        .ranged_attack_range = passive_config.ranged_attack_range,
    };
    auto *passive_unit = UnitFactory::create(passive_config, godot::Vector2(12.0, 34.0), passive_profile, passive_resolved_config);
    DEFN_REQUIRE(passive_unit != nullptr);
    DEFN_CHECK_EQ(passive_unit->get_unit_config().name, std::string("operator"));
    DEFN_CHECK_CLOSE(passive_unit->get_position().x, 12.0, 0.001);
    DEFN_CHECK_CLOSE(passive_unit->get_position().y, 34.0, 0.001);
    DEFN_CHECK_CLOSE(passive_unit->get_attack_range(), passive_config.melee_attack_range, 0.001);
    DEFN_CHECK_CLOSE(passive_unit->get_ranged_range(), passive_config.ranged_attack_range, 0.001);

    UnitFactory::initialize(passive_unit);
    const int initialized_child_count = passive_unit->get_child_count();
    UnitFactory::initialize(passive_unit);
    DEFN_CHECK_EQ(passive_unit->get_child_count(), initialized_child_count);
    DEFN_CHECK(unit_has_passive_factory_stack(passive_unit));

    UnitConfig combat_config = make_presenter_unit_config("jackal", 0);
    combat_config.side = UnitSide::HOSTILE;
    combat_config.melee_damage = 10;
    combat_config.move_speed_pixels_per_second = 60.0F;
    UnitRuntimeProfile combat_profile = UnitRuntimeProfile::combatant();
    combat_profile.enable_sound = false;

    SpawnUnitIntent request{
        .unit_id = "jackal",
        .side = MatchUnitSide::Hostile,
        .position = {.x = 80.0, .y = 90.0},
        .runtime_profile = combat_profile,
        .resolved_runtime_config = {.melee_attack_range = combat_config.melee_attack_range, .ranged_attack_range = combat_config.ranged_attack_range},
    };
    auto *combat_unit = UnitFactory::materialize(request, combat_config);
    DEFN_REQUIRE(combat_unit != nullptr);
    UnitFactory::initialize(combat_unit);
    DEFN_CHECK_EQ(combat_unit->get_side(), UnitSide::HOSTILE);
    DEFN_CHECK(unit_has_combat_factory_stack(combat_unit));

    memdelete(passive_unit);
    memdelete(combat_unit);
}

DEFN_TEST(menu_manager_builds_data_driven_menu_flows) {
    auto *menu_manager = memnew(MenuManager);
    menu_manager->_ready();

    DEFN_CHECK(menu_manager_shows_main_menu(menu_manager));
    DEFN_CHECK(menu_manager_background_covers_viewport(menu_manager));

    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::GotoMenu), "game_menu");
    DEFN_CHECK(menu_manager_shows_game_menu(menu_manager));

    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::GotoMenu), "options_menu");
    DEFN_CHECK(menu_manager_shows_options_menu(menu_manager));

    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::ShowLevelSelect), {});
    DEFN_CHECK(menu_manager_shows_level_select(menu_manager));

    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::ShowProgression), {});
    DEFN_CHECK(menu_manager_shows_progression(menu_manager));

    memdelete(menu_manager);
}

DEFN_TEST(progression_stats_screen_view_switches_dossiers_and_preserves_selection_across_owned_grid) {
    auto *view = memnew(ProgressionStatsScreenView);
    ProgressionOverviewSnapshot snapshot{
        .entities = {{.id = "base", .kind = ProgressionEntityKind::BASE, .unlocked = true},
                     {.id = "breacher", .kind = ProgressionEntityKind::UNIT, .unlocked = true, .stats = {{.id = "health", .effective_value = 400.0}}},
                     {.id = "marksman", .kind = ProgressionEntityKind::UNIT, .unlocked = false},
                     {.id = "operations", .kind = ProgressionEntityKind::OPERATIONS, .unlocked = true}}};
    view->configure(std::move(snapshot), {}, {});

    DEFN_CHECK(progression_view_has_initial_entity_state(view));

    view->select_entity("base");
    DEFN_CHECK(has_label_text(view, "Base"));
    view->show_owned_upgrades();
    DEFN_CHECK(has_label_text(view, "ALL OWNED UPGRADES"));
    DEFN_CHECK(has_label_containing(view, "No upgrades yet"));
    view->show_dossier();
    DEFN_CHECK(has_label_text(view, "Base"));

    memdelete(view);
}

DEFN_TEST(base_objective_configures_health_hitbox_and_optional_attack_stack) {
    auto *objective = BaseObjectiveFactory::create(250, godot::Vector2(300.0, 180.0));

    DEFN_CHECK_EQ(objective->get_current_hp(), 250);
    DEFN_CHECK_EQ(objective->get_max_hp(), 250);
    DEFN_CHECK(base_objective_has_basic_stack(objective));

    objective->take_damage(40);
    DEFN_CHECK_EQ(objective->get_current_hp(), 210);
    objective->take_damage(500);
    DEFN_CHECK(objective->is_dead());
    DEFN_CHECK_EQ(objective->get_current_hp(), 0);

    UnitConfig tower_config = make_presenter_unit_config("base", 0);
    tower_config.side = UnitSide::FRIENDLY;
    tower_config.ranged_damage = 15;
    tower_config.ranged_attack_range = 320.0F;
    tower_config.scale = 1.0F;
    tower_config.animations.clear();
    tower_config.animations.push_back({"idle", {.path_template = "res://assets/tower.png", .frame_count = 1, .loop = true}});
    tower_config.animations.push_back({"death", {.path_template = "res://assets/tower_destroyed.png", .frame_count = 1, .loop = false}});

    auto *armed_objective = BaseObjectiveFactory::create(300, godot::Vector2(500.0, 220.0), tower_config);
    DEFN_CHECK(base_objective_has_attack_stack(armed_objective));

    armed_objective->flash_damage(godot::Color(1.0, 0.0, 0.0));
    armed_objective->_process(1.0);

    memdelete(objective);
    memdelete(armed_objective);
}

DEFN_TEST(pause_menu_builds_buttons_and_toggles_tree_pause) {
    auto *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    DEFN_REQUIRE(tree != nullptr);
    auto *root = tree->get_root();
    DEFN_REQUIRE(root != nullptr);

    auto *pause_menu = memnew(PauseMenu);
    root->add_child(pause_menu);
    if (find_button_by_text(pause_menu, "Resume") == nullptr) {
        pause_menu->_ready();
    }

    DEFN_CHECK(pause_menu_has_expected_buttons(pause_menu));
    DEFN_CHECK(pause_menu_overlay_visible(pause_menu, false));

    pause_menu->toggle_pause();
    DEFN_CHECK(tree->is_paused());
    DEFN_CHECK(pause_menu_overlay_visible(pause_menu, true));

    pause_menu->toggle_pause();
    DEFN_CHECK(!tree->is_paused());
    DEFN_CHECK(pause_menu_overlay_visible(pause_menu, false));

    root->remove_child(pause_menu);
    memdelete(pause_menu);
}

DEFN_TEST(camera_scroll_controller_positions_triggers_and_updates_grid_camera) {
    const GameplayRules rules = make_camera_test_rules();
    CameraScrollController controller;
    controller.configure(rules, 3000.0F);

    DEFN_CHECK(camera_scroll_controller_has_initial_positions(controller));
    DEFN_CHECK(controller.advance_target());
    DEFN_CHECK_CLOSE(controller.get_camera_anchor_position().x, 750.0, 0.001);

    auto *camera = memnew(Camera2D);
    auto *grid = memnew(GridManager);
    camera->set_position(godot::Vector2(500.0, 300.0));
    controller.update_camera(camera, grid, 0.1);
    DEFN_CHECK_CLOSE(camera->get_position().x, 575.0, 0.001);
    DEFN_CHECK_CLOSE(grid->get_camera_x(), 575.0, 0.001);

    controller.update_camera(camera, grid, 1.0);
    DEFN_CHECK_CLOSE(camera->get_position().x, 750.0, 0.001);
    DEFN_CHECK_CLOSE(camera->get_position().y, 300.0, 0.001);

    DEFN_CHECK(controller.retreat_target());
    DEFN_CHECK_CLOSE(controller.get_camera_anchor_position().x, 500.0, 0.001);
    DEFN_CHECK(!controller.retreat_target());
    controller.update_camera(nullptr, grid, 1.0);
    controller.update_camera(camera, nullptr, 1.0);

    memdelete(camera);
    memdelete(grid);
}

DEFN_TEST(game_background_builder_builds_parallax_background_from_texture) {
    GameplayRules rules = make_camera_test_rules();
    rules.viewport_height = 360.0F;
    rules.world_multiplier = 3;

    const GameBackgroundBuildResult result = GameBackgroundBuilder::build("res://assets/backgrounds/middle_east_ruin_tiling.png", rules);
    DEFN_CHECK(background_build_matches_rules(result, rules));

    memdelete(result.background);
}

DEFN_TEST(attack_target_resolver_maps_battle_entities_and_rejects_plain_nodes) {
    DEFN_CHECK(resolve_attack_target(static_cast<Object *>(nullptr)) == nullptr);
    DEFN_CHECK(resolve_attack_target(ObjectID()) == nullptr);

    auto *plain_node = memnew(Node2D);
    DEFN_CHECK(resolve_attack_target(plain_node) == nullptr);

    auto *objective = BaseObjectiveFactory::create(100, godot::Vector2(10.0, 20.0));
    DEFN_CHECK(resolve_attack_target(objective) == static_cast<AttackTarget *>(objective));
    DEFN_CHECK(resolve_attack_target(objective->get_target_object_id()) == static_cast<AttackTarget *>(objective));

    memdelete(plain_node);
    memdelete(objective);
}

DEFN_TEST(projectile_attack_applies_direct_and_splash_damage_to_hostile_targets) {
    auto *parent = memnew(Node2D);
    auto *direct_target = add_test_objective(parent, UnitSide::HOSTILE, 100, godot::Vector2(100.0, 0.0));
    auto *splash_target = add_test_objective(parent, UnitSide::HOSTILE, 100, godot::Vector2(130.0, 0.0));
    auto *far_target = add_test_objective(parent, UnitSide::HOSTILE, 100, godot::Vector2(260.0, 0.0));
    auto *friendly_target = add_test_objective(parent, UnitSide::FRIENDLY, 100, godot::Vector2(120.0, 0.0));

    ProjectileAttackConfig config;
    config.speed_pixels_per_second = 0.0F;
    config.splash_radius = 60.0F;
    config.affected_fraction = 1.0F;
    config.min_affected_targets = 1;
    config.impact_damage = 40;
    config.splash_damage = 15;
    config.projectile_animation.frame_count = 0;
    config.explosion_animation.frame_count = 0;

    auto *projectile = ProjectileFactory::create(parent, config, UnitSide::FRIENDLY, godot::Color(1.0, 0.2, 0.1), godot::Vector2(0.0, 0.0),
                                                 direct_target->get_target_global_position(), direct_target, 25);
    projectile->_process(0.1);

    DEFN_CHECK_EQ(direct_target->get_current_hp(), 60);
    DEFN_CHECK_EQ(splash_target->get_current_hp(), 85);
    DEFN_CHECK_EQ(far_target->get_current_hp(), 100);
    DEFN_CHECK_EQ(friendly_target->get_current_hp(), 100);

    memdelete(parent);
}

} // namespace defn
