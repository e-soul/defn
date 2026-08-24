// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "attack_target_resolver.h"
#include "base_objective.h"
#include "base_objective_factory.h"
#ifdef DEFN_DEBUG_RENDERING_ENABLED
#include "belt_debug_overlay.h"
#endif
#include "animation_controller.h"
#include "camera_scroll_controller.h"
#include "campaign_map_view.h"
#include "combat_component.h"
#include "defn_balance_runner.h"
#include "defn_sim_runner.h"
#include "deploy_card_presenter.h"
#include "game_background_builder.h"
#include "grid_manager.h"
#include "health_component.h"
#include "hud.h"
#include "hud_meters.h"
#include "match_result_cutscene_view_model.h"
#include "menu_manager.h"
#include "pause_menu.h"
#include "progression_stat_meter.h"
#include "progression_stats_screen_view.h"
#include "projectile_attack.h"
#include "projectile_factory.h"
#include "reposition_destination_marker.h"
#include "score_screen_view.h"
#include "scripted_random_source.h"
#include "selection_indicator.h"
#include "unit.h"
#include "unit_factory.h"
#include "unit_selection_controller.h"
#include "upgrade_card_presenter.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/parallax2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace defn {

namespace {

template <typename ObjectType> struct GodotObjectDeleter {
    void operator()(ObjectType *object) const { memdelete(object); }
};

template <typename ObjectType> using GodotObjectOwner = std::unique_ptr<ObjectType, GodotObjectDeleter<ObjectType>>;

Window *scene_root() {
    auto *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    return tree == nullptr ? nullptr : tree->get_root();
}

// Mounts the node under the real scene root so `_ready()` paths that use `get_tree()` behave as they do in game.
template <typename NodeType> class TreeMountedNode {
  public:
    TreeMountedNode() : node_(memnew(NodeType)), root_(scene_root()) {
        if (root_ != nullptr) {
            root_->add_child(node_);
        }
    }

    TreeMountedNode(const TreeMountedNode &) = delete;
    TreeMountedNode &operator=(const TreeMountedNode &) = delete;
    TreeMountedNode(TreeMountedNode &&) = delete;
    TreeMountedNode &operator=(TreeMountedNode &&) = delete;

    ~TreeMountedNode() {
        if (root_ != nullptr) {
            root_->remove_child(node_);
        }
        memdelete(node_);
    }

    [[nodiscard]] NodeType *get() const { return node_; }

  private:
    NodeType *node_;
    Window *root_;
};

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

/// A card carries its title in a label inside the frame rather than as the button's own text, so it is found
/// by what it says rather than by a property Godot happens to draw.
Button *find_card_by_title(Node *root, const String &title) {
    for (auto *button : collect_buttons(root)) {
        if (has_label_text(button, title)) {
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

Node *find_node_named(Node *root, const String &name) {
    if (root == nullptr) {
        return nullptr;
    }
    if (String(root->get_name()) == name) {
        return root;
    }

    const int child_count = root->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index) {
        if (Node *found = find_node_named(root->get_child(child_index), name); found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

bool has_node_named(Node *root, const String &name) { return find_node_named(root, name) != nullptr; }

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

bool roster_button_uses_variation(Button *button, const char *variation) {
    return button != nullptr && button->get_theme_type_variation() == StringName(variation);
}

/// A card names its subject with a tinted theme mark, not a glyph from the machine's colour emoji font. An
/// unknown key still has to draw something, which is what `generic` is for.
bool card_shows_a_tinted_mark(Button *button) {
    std::vector<TextureRect *> marks;
    collect_nodes(button, marks);
    const auto is_icon = [](TextureRect *mark) { return mark->get_name() == StringName("Icon") && mark->get_texture().is_valid(); };
    return std::ranges::any_of(marks, is_icon);
}

bool selected_upgrade_button_matches(Button *button) {
    return button != nullptr && button->is_disabled() && button_minimum_size_is(button, 180.0, 220.0) &&
           button->get_theme_type_variation() == StringName("DefnCardSelectedButton") && card_shows_a_tinted_mark(button) &&
           has_all_labels(button, {"x2", "Rapid Reload", "Shoot more often."});
}

bool fallback_upgrade_button_matches(Button *button) {
    return button != nullptr && !button->is_disabled() && card_shows_a_tinted_mark(button) && has_all_labels(button, {"Upgrade"});
}

bool progression_view_has_initial_entity_state(ProgressionStatsScreenView *view) {
    Button *selected_button = find_card_by_title(view, "Breacher");
    Button *locked_button = find_card_by_title(view, "Marksman [Locked]");
    return has_label_text(view, "Breacher") && view->find_child("EntityPortraitFallback", true, false) != nullptr && selected_button != nullptr &&
           button_minimum_size_is(selected_button, 210.0, 74.0) && roster_button_uses_variation(selected_button, "DefnRosterSelectedButton") &&
           locked_button != nullptr && locked_button->is_disabled() && roster_button_uses_variation(locked_button, "DefnRosterButton");
}

bool progression_stat_meter_shows_exact_detail_on_request(ProgressionStatsScreenView *view) {
    auto *meter = Object::cast_to<ProgressionStatMeter>(view->find_child("StatMeter_health", true, false));
    auto *exact_detail = Object::cast_to<Label>(view->find_child("ExactStatDetail", true, false));
    if (meter == nullptr || exact_detail == nullptr) {
        return false;
    }

    const bool initially_hidden = !has_label_text(view, "400") && exact_detail->get_text().is_empty();
    meter->emit_signal("detail_state_changed", "health", "Health: 400 HP", true);
    const bool shown_on_request = exact_detail->get_text() == String("Health: 400 HP");
    meter->emit_signal("detail_state_changed", "health", "Health: 400 HP", false);
    return meter->get_segment_count() == 5 && meter->get_focus_mode() == Control::FOCUS_ALL && initially_hidden && shown_on_request &&
           exact_detail->get_text().is_empty();
}

bool score_screen_view_matches_victory_layout(Node *parent, const ScoreScreenViewNodes &view) {
    return view.overlay != nullptr && view.panel != nullptr && parent->get_child_count() == 1;
}

bool score_screen_has_victory_content(Node *overlay) {
    return has_all_labels(overlay, {"VICTORY", "FIRST CLEAR UPGRADE: Level 01", "Level 01 cleared for the first time.", "NEW UNLOCK: Level 02!",
                                    "YOUR UPGRADES", "Rapid Reload", "Owned Upgrade"});
}

bool score_screen_has_disabled_primary_actions(Node *overlay) {
    Button *next_button = find_button_by_text(overlay, "Next Level");
    Button *retry_button = find_button_by_text(overlay, "Retry");
    Button *campaign_button = find_button_by_text(overlay, "Campaign");
    return next_button != nullptr && retry_button != nullptr && campaign_button != nullptr && next_button->is_disabled() && retry_button->is_disabled() &&
           campaign_button->is_disabled();
}

bool hud_has_instrument_plates(HUD *hud) {
    return has_all_named_nodes(hud, {"EnergyPlate", "InfoPlate", "IntegrityPlate", "IntegrityMedallion", "IntegrityMeter"});
}

/// The three plates must sit level with each other, which the shared minimum height is what guarantees.
bool hud_plates_are_level(HUD *hud) {
    float shared = -1.0F;
    for (const char *name : {"EnergyPlate", "InfoPlate", "IntegrityPlate"}) {
        auto *plate = Object::cast_to<Control>(find_node_named(hud, String(name)));
        if (plate == nullptr) {
            return false;
        }
        const float height = plate->get_combined_minimum_size().y;
        if (shared < 0.0F) {
            shared = height;
        } else if (std::abs(shared - height) > 0.01F) {
            return false;
        }
    }
    return shared > 0.0F;
}

bool hud_has_initial_state(HUD *hud) { return has_all_labels(hud, {"ENERGY", "100", "WAVE", "1", "/ 3", "SCORE", "0", "INTEGRITY"}); }

bool hud_has_updated_match_state(HUD *hud) { return has_all_labels(hud, {"42", "125", "2", "/ 5", "FACTORY"}); }

template <typename MeterType> MeterType *find_meter(HUD *hud) {
    std::vector<MeterType *> meters;
    collect_nodes(hud, meters);
    return meters.empty() ? nullptr : meters.front();
}

bool hud_integrity_meter_shows(HUD *hud, int expected_segments) {
    auto *meter = find_meter<HudIntegrityMeter>(hud);
    return meter != nullptr && meter->get_segment_count() == expected_segments;
}

Button *find_deploy_card_button(Node *root) {
    for (auto *button : collect_buttons(root)) {
        if (button->get_text().is_empty()) {
            return button;
        }
    }

    return nullptr;
}

bool hud_has_operator_card(HUD *hud) { return has_label_text(hud, "Operator"); }

bool hud_deploy_card_disabled_for_resource(HUD *hud, int core_resource, bool expected_disabled) {
    hud->update_core_resource(core_resource);
    Button *deploy_button = find_deploy_card_button(hud);
    return deploy_button != nullptr && deploy_button->is_disabled() == expected_disabled;
}

bool unit_has_passive_factory_stack(Unit *unit) {
    return has_all_named_nodes(unit, {"HealthComponent", "HealthBarWidget", "AnimationController"}) && !has_node_named(unit, "MovementComponent") &&
           !has_node_named(unit, "CombatComponent");
}

bool unit_has_combat_factory_stack(Unit *unit) { return has_all_named_nodes(unit, {"DetectionComponent", "MovementComponent", "CombatComponent"}); }

// Entering the tree already triggers `_ready()`; only drive it manually when the node stayed detached.
MenuManager *ready_menu_manager(const TreeMountedNode<MenuManager> &owner) {
    MenuManager *menu_manager = owner.get();
    if (menu_manager->get_node_or_null("UILayer") == nullptr) {
        menu_manager->_ready();
    }
    return menu_manager;
}

/// The career score rides an `hud_pod` plate carrying a score readout, the same instrument the match HUD uses.
bool menu_manager_shows_career_score(MenuManager *menu_manager) {
    return has_node_named(menu_manager, "CareerScorePlate") && has_label_containing(menu_manager, "CAREER");
}

/// Every menu is built by `build_screen`, so it carries the same backdrop, panel and heading as the score and
/// progression screens rather than a bare stack of buttons.
bool menu_manager_screen_has_chrome(MenuManager *menu_manager, const char *title) {
    return has_node_named(menu_manager, "ScreenPanel") && has_node_named(menu_manager, "ScreenFooter") && has_all_labels(menu_manager, {title});
}

bool menu_manager_shows_main_menu(MenuManager *menu_manager) {
    return menu_manager_shows_career_score(menu_manager) && menu_manager_screen_has_chrome(menu_manager, "DEFN") &&
           has_all_buttons(menu_manager, {"Play", "Options", "Quit"});
}

bool menu_manager_shows_game_menu(MenuManager *menu_manager) {
    return menu_manager_screen_has_chrome(menu_manager, "CAMPAIGN") && has_all_buttons(menu_manager, {"New/Continue Game", "Progress", "Main Menu"});
}

bool menu_manager_shows_options_menu(MenuManager *menu_manager) {
    return menu_manager_screen_has_chrome(menu_manager, "OPTIONS") &&
           has_all_labels(menu_manager, {"Video", "Display Mode", "Resolution", "VSync", "Audio", "Master Volume"}) && has_all_buttons(menu_manager, {"Back"});
}

bool menu_manager_shows_level_select(MenuManager *menu_manager) {
    return has_all_labels(menu_manager, {"CAMPAIGN / THE EASTERN EXPEDITION", "ENEMY PRESENCE"}) && has_all_buttons(menu_manager, {"BACK"}) &&
           (find_button_by_text(menu_manager, "DEPLOY") != nullptr || find_button_by_text(menu_manager, "REPLAY") != nullptr);
}

bool menu_manager_shows_progression(MenuManager *menu_manager) {
    return has_all_labels(menu_manager, {"COMMAND ROSTER"}) && has_all_buttons(menu_manager, {"All Owned Upgrades", "Back"});
}

bool pump_campaign_map_loading(CampaignMapView *campaign_map) {
    if (campaign_map == nullptr) {
        return false;
    }
    for (int attempt = 0; attempt < 10000 && campaign_map->loading_state() != CampaignMapView::LoadingState::Ready &&
                          campaign_map->loading_state() != CampaignMapView::LoadingState::Failed;
         ++attempt) {
        campaign_map->_process(0.016);
        OS::get_singleton()->delay_usec(1000);
    }
    return campaign_map->loading_state() == CampaignMapView::LoadingState::Ready;
}

CampaignMapView *find_campaign_map(Node *root) {
    std::vector<CampaignMapView *> campaign_maps;
    collect_nodes(root, campaign_maps);
    return campaign_maps.size() == static_cast<std::size_t>(1) ? campaign_maps.front() : nullptr;
}

bool menu_manager_finishes_level_select_loading(MenuManager *menu_manager) {
    CampaignMapView *campaign_map = find_campaign_map(menu_manager);
    return pump_campaign_map_loading(campaign_map) && menu_manager_shows_level_select(menu_manager);
}

CampaignMapView *show_campaign_map(const TreeMountedNode<MenuManager> &owner) {
    MenuManager *menu_manager = ready_menu_manager(owner);
    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::ShowLevelSelect), {});
    CampaignMapView *campaign_map = find_campaign_map(menu_manager);
    (void)pump_campaign_map_loading(campaign_map);
    return campaign_map;
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

void check_state_medallion(CampaignMapView *campaign_map) {
    const String medallion_path = "ReferenceSurface/MapInteractionLayer/MissionNodes/level_01/StateMedallion";
    auto *medallion = Object::cast_to<Panel>(campaign_map->get_node_or_null(medallion_path));
    DEFN_REQUIRE(medallion != nullptr);
    DEFN_CHECK(medallion->has_theme_stylebox_override("panel"));

    auto *state_mark = Object::cast_to<TextureRect>(campaign_map->get_node_or_null(medallion_path + String("/StateMark")));
    DEFN_REQUIRE(state_mark != nullptr);
    DEFN_CHECK(state_mark->get_texture().is_valid());
    // The mark spans the medallion exactly, so it stays concentric with the ring at any node scale.
    DEFN_CHECK_CLOSE(static_cast<double>(state_mark->get_anchor(SIDE_RIGHT)), 1.0, 0.001);
    DEFN_CHECK_CLOSE(static_cast<double>(state_mark->get_anchor(SIDE_BOTTOM)), 1.0, 0.001);
    DEFN_CHECK(state_mark->get_modulate() != godot::Color(1, 1, 1, 1));
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
    DEFN_CHECK(button->get_theme_type_variation() == StringName("DefnDeployCardButton"));
    DEFN_CHECK(has_label_text(button, "Operator"));
    DEFN_CHECK(has_label_text(button, "25"));
    DEFN_CHECK(card_shows_a_tinted_mark(button));

    Label *title = find_label_by_text(button, "Operator");
    DEFN_REQUIRE(title != nullptr);
    DEFN_CHECK(title->get_theme_type_variation() == StringName("DefnCardTitleLabel"));

    memdelete(button);
    memdelete(receiver);
}

DEFN_TEST(upgrade_card_presenter_builds_selected_disabled_and_fallback_cards) {
    auto *receiver = memnew(Node);

    UpgradeCardViewModel upgrade;
    upgrade.id = "rapid_reload";
    upgrade.name = "Rapid Reload";
    upgrade.description = "Shoot more often.";
    upgrade.icon = "target";
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
    model.reward.available_upgrades.push_back({.id = "rapid_reload", .name = "Rapid Reload", .description = "Shoot more often.", .icon = "target"});
    model.owned_upgrades.push_back({.id = "owned", .name = "Owned Upgrade", .description = "Already claimed.", .icon = "salvage", .owned_count = 1});

    const Callable action = make_valid_callable(parent);
    const ScoreScreenViewNodes view =
        ScoreScreenView::show(parent, model, {.on_next_level = action, .on_retry = action, .on_campaign = action, .on_select_upgrade = action});

    DEFN_CHECK(score_screen_view_matches_victory_layout(parent, view));
    DEFN_CHECK(score_screen_has_victory_content(view.overlay));
    DEFN_CHECK(score_screen_has_disabled_primary_actions(view.overlay));
    DEFN_CHECK(view.overlay->find_child("ScreenScroll", true, false) == nullptr);

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
    DEFN_REQUIRE(find_button_by_text(view.overlay, "Campaign") != nullptr);

    memdelete(parent);
}

DEFN_TEST(hud_builds_instrument_pods_and_tracks_match_state) {
    const TreeMountedNode<HUD> owner;
    HUD *hud = owner.get();

    DEFN_CHECK(hud_has_instrument_plates(hud));
    DEFN_CHECK(hud_plates_are_level(hud));
    DEFN_CHECK(hud_has_initial_state(hud));

    hud->update_core_resource(42);
    hud->update_score(125);
    hud->update_wave(2, 5);
    hud->set_level("Factory");
    DEFN_CHECK(hud_has_updated_match_state(hud));

    hud->update_integrity(450, 500);
    DEFN_CHECK(hud_integrity_meter_shows(hud, 5));
}

DEFN_TEST(hud_plate_widths_hold_still_as_values_gain_digits) {
    const TreeMountedNode<HUD> owner;
    HUD *hud = owner.get();

    auto plate_width = [hud](const char *name) {
        auto *plate = Object::cast_to<Control>(find_node_named(hud, String(name)));
        return plate == nullptr ? -1.0F : plate->get_combined_minimum_size().x;
    };

    hud->set_level("Desert Outpost");
    hud->update_core_resource(5);
    hud->update_score(0);
    const float energy_width = plate_width("EnergyPlate");
    const float info_width = plate_width("InfoPlate");
    DEFN_REQUIRE(energy_width > 0.0F);
    DEFN_REQUIRE(info_width > 0.0F);

    // One, two and three digits all fit the reserved room, so neither plate moves.
    for (const int energy : {5, 42, 100}) {
        hud->update_core_resource(energy);
        DEFN_CHECK_CLOSE(plate_width("EnergyPlate"), energy_width, 0.01);
    }
    for (const int score : {0, 40, 480}) {
        hud->update_score(score);
        DEFN_CHECK_CLOSE(plate_width("InfoPlate"), info_width, 0.01);
    }

    // Past the floor a plate widens once and keeps the room, rather than shrinking back on the next tick.
    hud->update_core_resource(1000);
    const float widened = plate_width("EnergyPlate");
    DEFN_CHECK(widened > energy_width);
    hud->update_core_resource(7);
    DEFN_CHECK_CLOSE(plate_width("EnergyPlate"), widened, 0.01);
}

DEFN_TEST(hud_hides_the_level_reading_when_there_is_no_name) {
    const TreeMountedNode<HUD> owner;
    HUD *hud = owner.get();

    hud->set_level("Challenge");
    DEFN_CHECK(has_label_text(hud, "CHALLENGE"));

    hud->set_level({});
    Node *level_group = find_node_named(hud, "LevelGroup");
    DEFN_REQUIRE(level_group != nullptr);
    DEFN_CHECK(!Object::cast_to<Control>(level_group)->is_visible());
}

DEFN_TEST(hud_builds_deploy_cards_and_score_screen) {
    const TreeMountedNode<HUD> owner;
    HUD *hud = owner.get();

    UnitConfig operator_config = make_presenter_unit_config("operator", 25);
    hud->set_friendly_units({operator_config});
    DEFN_CHECK(hud_has_operator_card(hud));

    DEFN_CHECK(hud_deploy_card_disabled_for_resource(hud, 10, true));
    DEFN_CHECK(hud_deploy_card_disabled_for_resource(hud, 30, false));

    ScoreScreenModel summary;
    summary.victory = false;
    summary.current_level_id = "level_01";
    summary.hearts_total = 3;
    hud->show_score_screen(summary);
    DEFN_CHECK(has_label_text(hud, "DEFEAT"));
}

DEFN_TEST(hud_shows_and_hides_match_result_banner) {
    const TreeMountedNode<HUD> owner;
    HUD *hud = owner.get();

    hud->show_match_result_banner(MatchResultCutscenePresenter::build(true));
    DEFN_CHECK(has_label_text(hud, "AREA SECURED"));
    DEFN_CHECK(label_font_color_matches(hud, "AREA SECURED", godot::Color(0.2, 1.0, 0.3, 1.0)));
    hud->hide_match_result_banner();
    DEFN_CHECK(!has_label_text(hud, "AREA SECURED"));

    hud->show_match_result_banner(MatchResultCutscenePresenter::build(false));
    DEFN_CHECK(has_label_text(hud, "DEFEAT"));
    DEFN_CHECK(label_font_color_matches(hud, "DEFEAT", godot::Color(1.0, 0.2, 0.2, 1.0)));
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

namespace {

// Ten frames at ten frames per second, so one frame lasts 0.1s. No sprite: the timing combat depends on comes from the
// animation clock alone, which is exactly what these tests are pinning down.
UnitConfig make_animation_timing_config() {
    UnitConfig config;
    config.name = "timing";
    config.animations = {
        {"walk", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0}},
        {"attack", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = false, .windup_frames = 3}},
        {"shoot", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = false, .windup_frames = 2}},
    };
    return config;
}

AnimationController *add_test_animation_controller(Node2D *owner, const UnitConfig &config) {
    auto *animation = memnew(AnimationController);
    animation->set_name("AnimationController");
    owner->add_child(animation);
    animation->configure(owner, config, false);
    return animation;
}

} // namespace

DEFN_TEST(animation_controller_processes_every_frame_once_it_is_in_the_tree) {
    // The animation clock only advances from _process, so combat timing silently stops if processing is ever off.
    TreeMountedNode<Node2D> owner;
    DEFN_REQUIRE(owner.get() != nullptr);
    auto *animation = add_test_animation_controller(owner.get(), make_animation_timing_config());

    DEFN_CHECK(animation->is_processing());
}

DEFN_TEST(animation_controller_derives_the_attack_windup_from_the_animation_clock) {
    auto *owner = memnew(Node2D);
    auto *animation = add_test_animation_controller(owner, make_animation_timing_config());

    DEFN_CHECK(!animation->is_attack_animation_playing());

    animation->play_attack_animation();
    DEFN_CHECK_EQ(static_cast<int>(animation->get_anim_state()), static_cast<int>(UnitPose::ATTACK));
    DEFN_CHECK(animation->is_attack_windup_active());

    animation->_process(0.25); // frame 2, the last committed frame
    DEFN_CHECK(animation->is_attack_windup_active());

    animation->_process(0.10); // frame 3, the cancelable backswing
    DEFN_CHECK(!animation->is_attack_windup_active());

    memdelete(owner);
}

DEFN_TEST(animation_controller_stops_reporting_an_attack_once_the_animation_plays_out) {
    auto *owner = memnew(Node2D);
    auto *animation = add_test_animation_controller(owner, make_animation_timing_config());

    animation->play_attack_animation();
    animation->_process(0.95);
    DEFN_CHECK(animation->is_attack_animation_playing());

    animation->_process(0.10);
    DEFN_CHECK(!animation->is_attack_animation_playing());

    memdelete(owner);
}

DEFN_TEST(animation_controller_holds_a_pose_without_running_the_attack_animation) {
    auto *owner = memnew(Node2D);
    auto *animation = add_test_animation_controller(owner, make_animation_timing_config());

    animation->hold_anim_state(UnitPose::ATTACK);
    DEFN_CHECK_EQ(static_cast<int>(animation->get_anim_state()), static_cast<int>(UnitPose::ATTACK));
    DEFN_CHECK(!animation->is_attack_animation_playing());

    animation->_process(1.0);
    DEFN_CHECK(!animation->is_attack_animation_playing());

    memdelete(owner);
}

DEFN_TEST(animation_controller_defers_the_shoot_effect_to_its_spawn_frame) {
    auto *owner = memnew(Node2D);
    auto *animation = add_test_animation_controller(owner, make_animation_timing_config());

    animation->play_shoot_animation(false, 4);
    DEFN_CHECK(animation->is_attack_animation_playing());
    DEFN_CHECK(!animation->consume_shoot_effect_triggered());

    animation->_process(0.35); // frame 3
    DEFN_CHECK(!animation->consume_shoot_effect_triggered());

    animation->_process(0.10); // frame 4, the projectile leaves the muzzle
    DEFN_CHECK(animation->consume_shoot_effect_triggered());
    DEFN_CHECK(!animation->consume_shoot_effect_triggered());

    memdelete(owner);
}

DEFN_TEST(animation_controller_triggers_a_frame_zero_shoot_effect_at_once) {
    auto *owner = memnew(Node2D);
    auto *animation = add_test_animation_controller(owner, make_animation_timing_config());

    animation->play_shoot_animation(false, 0);
    DEFN_CHECK(animation->consume_shoot_effect_triggered());

    memdelete(owner);
}

DEFN_TEST(animation_controller_drops_a_pending_shoot_effect_when_the_pose_changes) {
    auto *owner = memnew(Node2D);
    auto *animation = add_test_animation_controller(owner, make_animation_timing_config());

    animation->play_shoot_animation(false, 4);
    animation->cancel_pending_attack_presentation();
    DEFN_CHECK_EQ(static_cast<int>(animation->get_anim_state()), static_cast<int>(UnitPose::WALK));
    DEFN_CHECK(!animation->is_attack_animation_playing());

    animation->_process(1.0);
    DEFN_CHECK(!animation->consume_shoot_effect_triggered());

    memdelete(owner);
}

DEFN_TEST(animation_controller_reports_no_attack_timing_for_a_unit_without_attack_animations) {
    UnitConfig config = make_animation_timing_config();
    config.animations = {{"walk", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0}}};

    auto *owner = memnew(Node2D);
    auto *animation = add_test_animation_controller(owner, config);

    // Nothing to time the shot against, so the effect fires immediately, as stationary objectives rely on.
    animation->play_shoot_animation(false, 4);
    DEFN_CHECK(animation->consume_shoot_effect_triggered());
    DEFN_CHECK(!animation->is_attack_animation_playing());

    memdelete(owner);
}

DEFN_TEST(health_component_reports_effective_damage_and_caps_overkill) {
    auto *health = memnew(HealthComponent);
    health->configure(20);
    DEFN_CHECK_EQ(health->take_damage(-5), 0);
    DEFN_CHECK_EQ(health->take_damage(7), 7);
    health->set_max_hp_and_heal(30);
    DEFN_CHECK_EQ(health->get_current_hp(), 30);
    DEFN_CHECK_EQ(health->get_max_hp(), 30);
    DEFN_CHECK_EQ(health->take_damage(10), 10);
    DEFN_CHECK_EQ(health->take_damage(100), 20);
    DEFN_CHECK_EQ(health->take_damage(1), 0);
    memdelete(health);
}

DEFN_TEST(friendly_combat_unit_promotes_once_and_updates_attack_periods) {
    UnitConfig config = make_presenter_unit_config("operator", 20);
    config.side = UnitSide::FRIENDLY;
    config.melee_damage = 10;
    config.melee_attack_period_seconds = 1.0;
    config.ranged_damage = 8;
    config.ranged_attack_period_seconds = 2.0;
    UnitRuntimeProfile profile = UnitRuntimeProfile::combatant();
    profile.enable_sound = false;
    const ResolvedUnitRuntimeConfig resolved{
        .melee_attack_range = config.melee_attack_range,
        .ranged_attack_range = config.ranged_attack_range,
    };
    auto *unit = UnitFactory::create(config, {}, profile, resolved, {});
    UnitFactory::initialize(unit);
    auto *health = godot::Object::cast_to<HealthComponent>(unit->get_node_or_null("HealthComponent"));
    DEFN_REQUIRE(health != nullptr);
    DEFN_CHECK_EQ(health->take_damage(60), 60);

    unit->record_effective_damage_dealt(500);
    DEFN_CHECK(unit->is_field_promoted());
    DEFN_CHECK_EQ(health->get_max_hp(), 132);
    DEFN_CHECK_EQ(health->get_current_hp(), 132);
    DEFN_CHECK_EQ(unit->resolve_outgoing_damage(10), 11);
    auto *insignia = godot::Object::cast_to<godot::Label>(unit->get_node_or_null("FieldPromotionView/FieldPromotionInsignia"));
    DEFN_REQUIRE(insignia != nullptr);
    DEFN_CHECK_CLOSE(insignia->get_position().x + (insignia->get_combined_minimum_size().x * 0.5F), config.health_bar_offset.x + 85.0F, 0.001);
    auto *combat = godot::Object::cast_to<CombatComponent>(unit->get_node_or_null("CombatComponent"));
    DEFN_REQUIRE(combat != nullptr);
    DEFN_CHECK_CLOSE(combat->get_runtime_config().melee_attack_period_seconds, 0.9, 0.000001);
    DEFN_CHECK_CLOSE(combat->get_runtime_config().ranged_attack_period_seconds, 1.8, 0.000001);
    unit->record_effective_damage_dealt(500);
    DEFN_CHECK_CLOSE(combat->get_runtime_config().melee_attack_period_seconds, 0.9, 0.000001);
    memdelete(unit);
}

DEFN_TEST(unit_selection_controller_selects_visible_sprite_and_clears_when_unit_exits_tree) {
    const TreeMountedNode<Node2D> host_owner;
    Node2D *host = host_owner.get();
    DEFN_REQUIRE(host != nullptr);

    auto *entity_container = memnew(Node2D);
    host->add_child(entity_container);
    auto *controller = memnew(UnitSelectionController);
    host->add_child(controller);
    controller->configure(entity_container);

    UnitConfig config = make_presenter_unit_config("operator", 20);
    config.side = UnitSide::FRIENDLY;
    config.move_speed_pixels_per_second = 60.0F;
    UnitRuntimeProfile profile = UnitRuntimeProfile::combatant();
    profile.enable_sound = false;
    const ResolvedUnitRuntimeConfig resolved{
        .melee_attack_range = config.melee_attack_range,
        .ranged_attack_range = config.ranged_attack_range,
    };
    Unit *unit = UnitFactory::create(config, {}, profile, resolved, {});
    unit->set_position({300.0F, 300.0F});
    entity_container->add_child(unit);

    Ref<InputEventMouseButton> click;
    click.instantiate();
    click->set_button_index(MOUSE_BUTTON_LEFT);
    click->set_pressed(true);
    click->set_position({325.0F, 300.0F});
    controller->_unhandled_input(click);
    DEFN_CHECK(controller->has_selection());
    auto *indicator = Object::cast_to<Node2D>(unit->get_node_or_null("SelectionIndicator"));
    DEFN_REQUIRE(indicator != nullptr);
    DEFN_CHECK(indicator->is_visible());
    DEFN_CHECK_EQ(indicator->get_z_index(), 0);
    DEFN_CHECK(indicator->is_draw_behind_parent_enabled());
    DEFN_CHECK(indicator->get_global_position().y > unit->get_global_position().y + 40.0F);

    entity_container->remove_child(unit);
    DEFN_CHECK(!controller->has_selection());
    DEFN_CHECK(controller->get_node_or_null("SelectionIndicator") != nullptr);
    memdelete(unit);
}

DEFN_TEST(unit_selection_controller_previews_hovered_friendly_with_configured_style) {
    const TreeMountedNode<Node2D> host_owner;
    Node2D *host = host_owner.get();
    DEFN_REQUIRE(host != nullptr);

    auto *entity_container = memnew(Node2D);
    host->add_child(entity_container);
    auto *controller = memnew(UnitSelectionController);
    host->add_child(controller);
    controller->configure(entity_container);

    UnitConfig config = make_presenter_unit_config("operator", 20);
    config.side = UnitSide::FRIENDLY;
    config.move_speed_pixels_per_second = 60.0F;
    UnitRuntimeProfile profile = UnitRuntimeProfile::combatant();
    profile.enable_sound = false;
    const ResolvedUnitRuntimeConfig resolved{
        .melee_attack_range = config.melee_attack_range,
        .ranged_attack_range = config.ranged_attack_range,
    };
    Unit *unit = UnitFactory::create(config, {}, profile, resolved, {});
    unit->set_position({300.0F, 300.0F});
    entity_container->add_child(unit);

    Ref<InputEventMouseMotion> hover;
    hover.instantiate();
    hover->set_position({325.0F, 300.0F});
    controller->_unhandled_input(hover);

    auto *hover_indicator = Object::cast_to<SelectionIndicator>(unit->get_node_or_null("HoverIndicator"));
    DEFN_REQUIRE(hover_indicator != nullptr);
    DEFN_CHECK(hover_indicator->is_visible());
    DEFN_CHECK_CLOSE(hover_indicator->get_radius_x(), 26.0, 0.001);
    DEFN_CHECK_CLOSE(hover_indicator->get_radius_y(), 8.0, 0.001);
    DEFN_CHECK_CLOSE(hover_indicator->get_fill_color().a, 0.072, 0.001);
    DEFN_CHECK_CLOSE(hover_indicator->get_border_color().a, 0.475, 0.001);
    DEFN_CHECK(hover_indicator->get_global_position().y > unit->get_global_position().y + 40.0F);

    hover->set_position({900.0F, 600.0F});
    controller->_unhandled_input(hover);
    auto *detached_hover_indicator = Object::cast_to<SelectionIndicator>(controller->get_node_or_null("HoverIndicator"));
    DEFN_REQUIRE(detached_hover_indicator != nullptr);
    DEFN_CHECK(!detached_hover_indicator->is_visible());

    entity_container->remove_child(unit);
    memdelete(unit);
}

DEFN_TEST(unit_selection_controller_shows_pulsing_destination_marker_for_accepted_order) {
    const TreeMountedNode<Node2D> host_owner;
    Node2D *host = host_owner.get();
    DEFN_REQUIRE(host != nullptr);

    auto *entity_container = memnew(Node2D);
    host->add_child(entity_container);
    auto *controller = memnew(UnitSelectionController);
    host->add_child(controller);
    controller->configure(entity_container);

    UnitConfig config = make_presenter_unit_config("operator", 20);
    config.side = UnitSide::FRIENDLY;
    config.move_speed_pixels_per_second = 60.0F;
    UnitRuntimeProfile profile = UnitRuntimeProfile::combatant();
    profile.enable_sound = false;
    const ResolvedUnitRuntimeConfig resolved{
        .melee_attack_range = config.melee_attack_range,
        .ranged_attack_range = config.ranged_attack_range,
    };
    Unit *unit = UnitFactory::create(config, {}, profile, resolved, {});
    unit->set_position({300.0F, 300.0F});
    entity_container->add_child(unit);
    controller->select_unit(unit);

    Ref<InputEventMouseButton> order;
    order.instantiate();
    order->set_button_index(MOUSE_BUTTON_LEFT);
    order->set_pressed(true);
    order->set_position({250.0F, 450.0F});
    controller->_unhandled_input(order);

    auto *selection_indicator = Object::cast_to<SelectionIndicator>(unit->get_node_or_null("SelectionIndicator"));
    DEFN_REQUIRE(selection_indicator != nullptr);
    auto *destination_marker = Object::cast_to<RepositionDestinationMarker>(entity_container->get_node_or_null("RepositionDestinationMarker"));
    DEFN_REQUIRE(destination_marker != nullptr);
    DEFN_CHECK_CLOSE(destination_marker->get_global_position().x, 250.0, 0.001);
    DEFN_CHECK_CLOSE(destination_marker->get_global_position().y, selection_indicator->get_global_position().y, 0.001);
    DEFN_CHECK_CLOSE(destination_marker->get_radius_x(), 14.0, 0.001);
    DEFN_CHECK_EQ(destination_marker->get_pulse_count(), 3);
    destination_marker->_process(0.14);
    DEFN_CHECK(destination_marker->get_scale().x > 0.65F);
    destination_marker->_process(0.71);
    DEFN_CHECK(destination_marker->is_queued_for_deletion());

    entity_container->remove_child(unit);
    memdelete(unit);
}

DEFN_TEST(field_promotion_audio_resource_loads) {
    const godot::Ref<godot::AudioStream> stream = godot::ResourceLoader::get_singleton()->load("res://assets/sfx/field_promotion_kalimba.wav");
    DEFN_CHECK(stream.is_valid());
}

DEFN_TEST(menu_manager_builds_data_driven_menu_flows) {
    const TreeMountedNode<MenuManager> menu_manager_owner;
    auto *menu_manager = ready_menu_manager(menu_manager_owner);

    DEFN_CHECK(menu_manager_shows_main_menu(menu_manager));
    DEFN_CHECK(menu_manager_background_covers_viewport(menu_manager));

    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::GotoMenu), "game_menu");
    DEFN_CHECK(menu_manager_shows_game_menu(menu_manager));

    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::GotoMenu), "options_menu");
    DEFN_CHECK(menu_manager_shows_options_menu(menu_manager));

    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::ShowLevelSelect), {});
    DEFN_CHECK(menu_manager_finishes_level_select_loading(menu_manager));

    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::ShowProgression), {});
    DEFN_CHECK(menu_manager_shows_progression(menu_manager));
    std::vector<CampaignMapView *> campaign_maps;
    collect_nodes(menu_manager, campaign_maps);
    DEFN_REQUIRE(campaign_maps.size() == 1);
    DEFN_CHECK(campaign_maps.front()->is_queued_for_deletion());
}

DEFN_TEST(campaign_map_mounts_loading_overlay_before_composing_content) {
    const TreeMountedNode<MenuManager> menu_manager_owner;
    auto *menu_manager = ready_menu_manager(menu_manager_owner);
    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::ShowLevelSelect), {});

    std::vector<CampaignMapView *> campaign_maps;
    collect_nodes(menu_manager, campaign_maps);
    DEFN_REQUIRE(campaign_maps.size() == 1);
    CampaignMapView *campaign_map = campaign_maps.front();
    DEFN_CHECK_EQ(campaign_map->loading_state(), CampaignMapView::LoadingState::WaitingToStart);
    DEFN_CHECK(campaign_map->get_node_or_null("LoadingOverlay") != nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface") == nullptr);

    DEFN_CHECK(pump_campaign_map_loading(campaign_map));
    DEFN_CHECK_EQ(campaign_map->loading_state(), CampaignMapView::LoadingState::Ready);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface") != nullptr);
}

DEFN_TEST(campaign_map_loading_failure_shows_retry_and_back_actions) {
    GodotObjectOwner<CampaignMapView> campaign_map_owner(memnew(CampaignMapView));
    CampaignMapView *campaign_map = campaign_map_owner.get();
    campaign_map->configure(static_cast<ProgressionService *>(nullptr), {}, {}, nullptr);

    (void)pump_campaign_map_loading(campaign_map);
    DEFN_CHECK_EQ(campaign_map->loading_state(), CampaignMapView::LoadingState::Failed);
    DEFN_CHECK(find_button_by_text(campaign_map, "Retry") != nullptr);
    DEFN_CHECK(find_button_by_text(campaign_map, "Back") != nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface") == nullptr);
}

DEFN_TEST(campaign_map_loading_selects_the_presented_initial_mission) {
    GodotObjectOwner<CampaignMapView> campaign_map_owner(memnew(CampaignMapView));
    CampaignMapView *campaign_map = campaign_map_owner.get();
    const CampaignTextureDefinition texture{.path = "res://assets/campaign/desert_outpost_preview.jpg"};
    CampaignMapViewModel view_model{
        .background = texture,
        .missions = {{.level_id = "level_01", .name = "First", .preview = {.texture = texture}},
                     {.level_id = "level_02", .name = "Second", .preview = {.texture = texture}}},
        .initial_selected_level_id = "level_02",
    };
    campaign_map->configure(std::move(view_model), {}, {}, nullptr);

    DEFN_CHECK(pump_campaign_map_loading(campaign_map));
    DEFN_CHECK_EQ(campaign_map->loading_state(), CampaignMapView::LoadingState::Ready);
    DEFN_CHECK_EQ(campaign_map->selected_level_id(), std::string("level_02"));
    DEFN_REQUIRE(campaign_map->dossier() != nullptr);
}

DEFN_TEST(campaign_map_panorama_fills_and_clips_reference_surface) {
    const TreeMountedNode<MenuManager> menu_manager_owner;
    CampaignMapView *campaign_map = show_campaign_map(menu_manager_owner);

    DEFN_REQUIRE(campaign_map != nullptr);
    DEFN_CHECK(Object::cast_to<CanvasLayer>(campaign_map->get_parent()) != nullptr);
    auto *reference_surface = Object::cast_to<Control>(campaign_map->get_node_or_null("ReferenceSurface"));
    DEFN_REQUIRE(reference_surface != nullptr);
    DEFN_CHECK(reference_surface->is_clipping_contents());
    auto *panorama = Object::cast_to<Sprite2D>(campaign_map->get_node_or_null("ReferenceSurface/Panorama"));
    DEFN_REQUIRE(panorama != nullptr);
    DEFN_REQUIRE(panorama->get_texture().is_valid());
    DEFN_CHECK_CLOSE(static_cast<double>(panorama->get_texture()->get_width()) * panorama->get_scale().x, 1920.0, 0.001);
    DEFN_CHECK_CLOSE(static_cast<double>(panorama->get_texture()->get_height()) * panorama->get_scale().y, 1080.0, 0.001);
}

DEFN_TEST(campaign_map_uses_compact_preview_nodes_without_auxiliary_navigation_controls) {
    const TreeMountedNode<MenuManager> menu_manager_owner;
    CampaignMapView *campaign_map = show_campaign_map(menu_manager_owner);

    DEFN_REQUIRE(campaign_map != nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface/CloseButton") == nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface/HintsBackplate") == nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface/InputHints") == nullptr);
    auto *desert_node = Object::cast_to<Control>(campaign_map->get_node_or_null("ReferenceSurface/MapInteractionLayer/MissionNodes/level_01"));
    DEFN_REQUIRE(desert_node != nullptr);
    DEFN_CHECK_EQ(desert_node->get_size(), godot::Vector2(188.0F, 134.0F));
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface/MapInteractionLayer/MissionNodes/level_01/LabelPlate") == nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface/MapInteractionLayer/MissionNodes/level_01/MissionName") == nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface/MapInteractionLayer/MissionNodes/level_01/MissionDetail") == nullptr);
}

DEFN_TEST(campaign_map_preview_requires_click_and_double_click_deploys) {
    GodotObjectOwner<CampaignMapView> campaign_map_owner(memnew(CampaignMapView));
    GodotObjectOwner<Button> deployment_recorder(memnew(Button));
    CampaignMapView *campaign_map = campaign_map_owner.get();
    const CampaignTextureDefinition texture{.path = "res://assets/campaign/desert_outpost_preview.jpg"};
    CampaignMapViewModel view_model{
        .background = texture,
        .missions = {{.level_id = "level_01", .name = "First", .preview = {.texture = texture}, .state = CampaignNodeState::AVAILABLE},
                     {.level_id = "level_02", .name = "Second", .preview = {.texture = texture}, .state = CampaignNodeState::AVAILABLE}},
        .initial_selected_level_id = "level_02",
    };
    campaign_map->configure(std::move(view_model), Callable(deployment_recorder.get(), "set_text"), {}, nullptr);

    DEFN_REQUIRE(pump_campaign_map_loading(campaign_map));
    auto *first = Object::cast_to<Button>(campaign_map->get_node_or_null("ReferenceSurface/MapInteractionLayer/MissionNodes/level_01/Interaction"));
    auto *second = Object::cast_to<Button>(campaign_map->get_node_or_null("ReferenceSurface/MapInteractionLayer/MissionNodes/level_02/Interaction"));
    DEFN_REQUIRE(first != nullptr);
    DEFN_REQUIRE(second != nullptr);

    first->emit_signal("mouse_entered");
    DEFN_CHECK_EQ(campaign_map->selected_level_id(), std::string("level_02"));

    first->emit_signal("pressed");
    DEFN_CHECK_EQ(campaign_map->selected_level_id(), std::string("level_01"));
    DEFN_CHECK(deployment_recorder->get_text().is_empty());

    Ref<InputEventMouseButton> double_click;
    double_click.instantiate();
    double_click->set_button_index(MOUSE_BUTTON_LEFT);
    double_click->set_pressed(true);
    double_click->set_double_click(true);
    second->emit_signal("gui_input", double_click);
    DEFN_CHECK_EQ(campaign_map->selected_level_id(), std::string("level_02"));
    DEFN_CHECK_EQ(deployment_recorder->get_text(), String("level_02"));
}

DEFN_TEST(campaign_map_uses_readable_state_and_enemy_treatments) {
    const TreeMountedNode<MenuManager> menu_manager_owner;
    CampaignMapView *campaign_map = show_campaign_map(menu_manager_owner);

    DEFN_REQUIRE(campaign_map != nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("ReferenceSurface/MapInteractionLayer/MissionNodes/level_04/PostcardFrame") != nullptr);
    check_state_medallion(campaign_map);
    auto *node_interaction = Object::cast_to<Button>(campaign_map->get_node_or_null("ReferenceSurface/MapInteractionLayer/MissionNodes/level_01/Interaction"));
    DEFN_REQUIRE(node_interaction != nullptr);
    DEFN_CHECK_EQ(node_interaction->get_focus_mode(), Control::FOCUS_NONE);
    DEFN_CHECK(!node_interaction->has_theme_stylebox_override("focus"));
    DEFN_CHECK(has_label_text(campaign_map, "Grime"));
    DEFN_CHECK(!has_label_text(campaign_map, "[Grime]"));
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
    DEFN_CHECK(progression_stat_meter_shows_exact_detail_on_request(view));

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

    (void)objective->take_damage(40);
    DEFN_CHECK_EQ(objective->get_current_hp(), 210);
    (void)objective->take_damage(500);
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

    Vector2 camera_position{.x = 500.0F, .y = 300.0F};
    camera_position = controller.next_camera_position(camera_position, 0.1);
    DEFN_CHECK_CLOSE(camera_position.x, 575.0, 0.001);

    camera_position = controller.next_camera_position(camera_position, 1.0);
    DEFN_CHECK_CLOSE(camera_position.x, 750.0, 0.001);
    DEFN_CHECK_CLOSE(camera_position.y, 300.0, 0.001);

    DEFN_CHECK(controller.retreat_target());
    DEFN_CHECK_CLOSE(controller.get_camera_anchor_position().x, 500.0, 0.001);
    DEFN_CHECK(!controller.retreat_target());
}

// The kernel is covered natively; this covers the plumbing around it -- reading a checked-in scenario, loading the
// shipped content through the real loaders, and writing one JSON line per seed.
DEFN_TEST(defn_sim_runner_runs_a_checked_in_scenario_and_writes_jsonl) {
    Dictionary args;
    args["scenario"] = "res://scenarios/level_01_greedy.json";
    args["seeds"] = 2;
    args["out"] = "user://defn_sim_runner_test.jsonl";

    const Dictionary result = DefnSimRunner::run_sweep(args);

    DEFN_REQUIRE(bool(result.get("success", false)));
    DEFN_CHECK_EQ(int(result.get("runs", 0)), 2);

    const String written = FileAccess::get_file_as_string("user://defn_sim_runner_test.jsonl");
    DEFN_CHECK(!written.is_empty());
    DEFN_CHECK_EQ(int(written.strip_edges().split("\n").size()), 2);
    DEFN_CHECK(written.contains("\"level_id\":\"level_01\""));
    DEFN_CHECK(written.contains("\"policy\":\"greedy\""));
    DEFN_CHECK(written.contains("\"peak_window_5s\":"));
}

// The measurement itself is a lab result, not a rule, so this only pins that it runs and stays discriminating: a
// reference that takes no damage from anything would silently report every threat as zero, which is how the first
// version of it was wrong.
DEFN_TEST(defn_balance_runner_measures_a_discriminating_threat_ladder) {
    Dictionary args;
    args["seeds"] = 3;

    const Dictionary result = DefnBalanceRunner::measure(args);
    DEFN_REQUIRE(bool(result.get("success", false)));

    const Array threat = result.get("threat", Array());
    DEFN_CHECK_EQ(int(threat.size()), 4);

    double baseline = 0.0;
    double highest = 0.0;
    for (const Variant &value : threat) {
        const Dictionary row = value;
        const auto cost = static_cast<double>(row["cost_per_kill"]);
        DEFN_CHECK(cost > 0.0);
        if (String(row["unit_id"]) == String("grime")) {
            baseline = cost;
        }
        highest = std::max(highest, cost);
    }

    DEFN_CHECK(baseline > 0.0);
    // Grime must remain the cheapest threat, and the spread wide enough to be worth reporting.
    DEFN_CHECK(highest > baseline * 1.5);

    const Array roster = result.get("roster", Array());
    DEFN_CHECK_EQ(int(roster.size()), 4);
}

DEFN_TEST(defn_sim_runner_reports_a_missing_scenario) {
    Dictionary args;
    args["scenario"] = "res://scenarios/nosuchscenario.json";

    DEFN_CHECK(!bool(DefnSimRunner::run_sweep(args).get("success", false)));
}

DEFN_TEST(grid_manager_resolves_level_belt_width_ratios_to_screen_coordinates) {
    GameplayRules rules;
    rules.viewport_height = 800.0F;

    auto *grid = memnew(GridManager);
    grid->configure(rules, 0.75F, 0.25F);

    DEFN_CHECK_CLOSE(grid->get_rules().belt_top_y, 200.0, 0.001);
    DEFN_CHECK_CLOSE(grid->get_rules().belt_bottom_y, 600.0, 0.001);
    memdelete(grid);
}

DEFN_TEST(grid_manager_samples_belt_y_through_the_random_source_port) {
    GameplayRules rules;
    rules.viewport_height = 800.0F;

    auto *grid = memnew(GridManager);
    grid->configure(rules, 0.25F, 0.75F);

    tests::ScriptedRandomSource random;
    random.push_real(275.0F);
    random.push_real(425.0F);
    grid->set_random_source(&random);

    DEFN_CHECK_CLOSE(grid->sample_belt_y(), 275.0, 0.001);
    DEFN_CHECK_CLOSE(grid->sample_belt_y(), 425.0, 0.001);

    // Clearing the override falls back to the built-in source, which must still stay inside the belt.
    grid->set_random_source(nullptr);
    const double fallback_y = grid->sample_belt_y();
    DEFN_CHECK(fallback_y >= 200.0);
    DEFN_CHECK(fallback_y <= 600.0);

    memdelete(grid);
}

#ifdef DEFN_DEBUG_RENDERING_ENABLED
DEFN_TEST(belt_debug_overlay_starts_hidden_and_toggles_visibility) {
    auto *overlay = memnew(BeltDebugOverlay);

    DEFN_CHECK(!overlay->is_visible());
    overlay->toggle_visibility();
    DEFN_CHECK(overlay->is_visible());
    overlay->toggle_visibility();
    DEFN_CHECK(!overlay->is_visible());

    memdelete(overlay);
}
#endif

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

    auto *projectile = ProjectileFactory::create(parent, config, UnitSide::FRIENDLY, godot::ObjectID(), godot::Color(1.0, 0.2, 0.1), godot::Vector2(0.0, 0.0),
                                                 direct_target->get_target_global_position(), direct_target, 25);
    projectile->_process(0.1);

    DEFN_CHECK_EQ(direct_target->get_current_hp(), 60);
    DEFN_CHECK_EQ(splash_target->get_current_hp(), 85);
    DEFN_CHECK_EQ(far_target->get_current_hp(), 100);
    DEFN_CHECK_EQ(friendly_target->get_current_hp(), 100);

    memdelete(parent);
}

} // namespace defn
