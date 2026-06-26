#include "game_manager.h"

#include "base_objective.h"
#include "collision_layers.h"
#include "data_paths.h"
#include "game_background_builder.h"
#include "grid_manager.h"
#include "hud.h"
#include "match_director.h"
#include "pause_menu.h"
#include "progression_manager.h"
#include "scene_navigator.h"
#include "unit.h"
#include "unit_factory.h"
#include "vfx/bounty_energy_effect.h"
#include <algorithm>
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr real_t HALF = 2.0F;

} // namespace

GameManager::GameManager() = default;

void GameManager::_bind_methods() {}

void GameManager::_ready() {
    UtilityFunctions::print("GameManager: Initializing belt scroller game...");

    auto *progression = CampaignService::get_singleton();
    String level_id = progression->get_current_level_id();
    const String level_path = DataPaths::level_definition(level_id);

    // Load unit data from JSON
    unit_data_.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS);

    if (auto *grid = GridManager::get_singleton()) {
        grid->configure(unit_data_.get_globals().gameplay_rules);
        camera_scroll_controller_.configure(grid->get_rules(), grid->get_world_width());
    }

    match_director_.configure(progression, &unit_data_, GridManager::get_singleton());
    if (!match_director_.load_level(level_path)) {
        UtilityFunctions::printerr("GameManager: Failed to load level: ", level_path);
        return;
    }
    match_director_.begin_match();

    // Setup camera and visual layers using background from level data
    String bg_path = match_director_.get_background_path();
    if (bg_path.is_empty()) {
        bg_path = DataPaths::DEFAULT_GAME_BACKGROUND;
    }
    setup_background(bg_path);
    setup_camera();

    // Entity container (y-sort so closer-to-bottom entities render in front)
    entity_container = memnew(Node2D);
    entity_container->set_name("EntityContainer");
    entity_container->set_y_sort_enabled(true);
    add_child(entity_container);

    // HUD
    hud = memnew(HUD);
    hud->set_name("HUD");
    add_child(hud);

    hud->set_friendly_units(match_director_.build_available_friendlies());
    hud->set_level(match_director_.get_level_number(), match_director_.get_level_name());
    hud->update_core_resource(match_director_.get_core_resource());
    hud->update_wave(1, match_director_.get_total_waves());
    hud->update_hearts(match_director_.get_base_integrity());
    hud->update_card_affordability(match_director_.get_core_resource());
    hud->update_score(0);
    hud->connect("deploy_requested", callable_mp(this, &GameManager::on_deploy_requested));
    hud->connect("score_screen_next_level", callable_mp(this, &GameManager::on_score_screen_next_level));
    hud->connect("score_screen_retry", callable_mp(this, &GameManager::on_score_screen_retry));
    hud->connect("score_screen_main_menu", callable_mp(this, &GameManager::on_score_screen_main_menu));
    hud->connect("score_screen_upgrade_selected", callable_mp(this, &GameManager::on_score_screen_upgrade_selected));

    setup_base_objective();
    setup_scroll_triggers();

    // Core resource generation timer
    core_resource_timer = memnew(Timer);
    core_resource_timer->set_wait_time(1.0);
    core_resource_timer->set_one_shot(false);
    core_resource_timer->connect("timeout", callable_mp(this, &GameManager::on_core_resource_tick));
    add_child(core_resource_timer);
    core_resource_timer->start();

    // Pause menu (ESC to toggle)
    auto *pause_menu = memnew(PauseMenu);
    pause_menu->set_name("PauseMenu");
    add_child(pause_menu);
}

void GameManager::_process(double delta) {
    if (match_director_.is_game_over()) {
        return;
    }

    update_camera_scroll(delta);
    apply_match_update(match_director_.update(delta));
}

void GameManager::_input(const Ref<InputEvent> & /*event*/) {
    // Deployment is handled through HUD card buttons
}

void GameManager::setup_background(const String &bg_path) {
    auto *grid = GridManager::get_singleton();
    const auto &rules = grid->get_rules();

    const GameBackgroundBuildResult background = GameBackgroundBuilder::build(bg_path, rules);
    if (background.background == nullptr) {
        return;
    }

    grid->set_world_width(background.world_width);
    camera_scroll_controller_.configure(rules, background.world_width);
    add_child(background.background);
}

void GameManager::setup_camera() {
    const auto &rules = GridManager::get_singleton()->get_rules();
    camera = memnew(Camera2D);
    camera->set_name("Camera");

    camera->set_position(camera_scroll_controller_.get_camera_anchor_position());

    camera->set_limit(SIDE_LEFT, 0);
    camera->set_limit(SIDE_TOP, 0);
    camera->set_limit(SIDE_RIGHT, static_cast<int32_t>(camera_scroll_controller_.get_world_width()));
    camera->set_limit(SIDE_BOTTOM, static_cast<int32_t>(rules.viewport_height));

    add_child(camera);
}

void GameManager::update_camera_scroll(double delta) { camera_scroll_controller_.update_camera(camera, GridManager::get_singleton(), delta); }

void GameManager::setup_base_objective() {
    if (entity_container == nullptr) {
        return;
    }

    const std::optional<UnitConfig> base_visual_config = unit_data_.get_unit("base");

    base_objective = memnew(BaseObjective);
    base_objective->set_name("BaseObjective");
    base_objective->connect("durability_changed", callable_mp(this, &GameManager::on_base_durability_changed));
    base_objective->connect("objective_destroyed", callable_mp(this, &GameManager::on_base_destroyed));
    entity_container->add_child(base_objective);
    base_objective->configure(match_director_.get_base_max_health(), get_base_objective_position(), base_visual_config);
}

Area2D *GameManager::create_scroll_trigger(const String &name, uint32_t collision_mask) {
    auto *trigger = memnew(Area2D);
    trigger->set_name(name);
    trigger->set_collision_layer(CollisionLayers::NONE);
    trigger->set_collision_mask(collision_mask);
    trigger->set_monitoring(true);
    trigger->set_monitorable(false);

    auto *shape_node = memnew(CollisionShape2D);
    Ref<RectangleShape2D> rect;
    rect.instantiate();
    // Tall vertical strip covering well beyond the belt area
    rect->set_size(Vector2(20.0, camera_scroll_controller_.get_trigger_height()));
    shape_node->set_shape(rect);
    trigger->add_child(shape_node);

    add_child(trigger);
    return trigger;
}

void GameManager::setup_scroll_triggers() {
    right_scroll_trigger = create_scroll_trigger("RightScrollTrigger", CollisionLayers::RIGHT_SCROLL_TRIGGER_MASK);
    left_scroll_trigger = create_scroll_trigger("LeftScrollTrigger", CollisionLayers::LEFT_SCROLL_TRIGGER_MASK);

    update_scroll_trigger_positions();

    right_scroll_trigger->connect("area_entered", callable_mp(this, &GameManager::on_scroll_triggered).bind(false));
    left_scroll_trigger->connect("area_entered", callable_mp(this, &GameManager::on_scroll_triggered).bind(true));
}

void GameManager::update_scroll_trigger_positions() {
    if (right_scroll_trigger != nullptr) {
        right_scroll_trigger->set_position(camera_scroll_controller_.get_right_trigger_position());
    }

    if (left_scroll_trigger != nullptr) {
        left_scroll_trigger->set_position(camera_scroll_controller_.get_left_trigger_position());
    }
}

Vector2 GameManager::get_base_objective_position() {
    auto *grid = GridManager::get_singleton();
    if (grid == nullptr) {
        return {};
    }

    const auto &rules = grid->get_rules();
    const real_t objective_x = rules.breach_x + 96.0F;
    const real_t objective_y = (rules.belt_top_y + rules.belt_bottom_y) / HALF;
    return {objective_x, objective_y};
}

bool GameManager::is_valid_scroll_trigger_unit(Area2D *area, const char *required_group) {
    if (area == nullptr) {
        return false;
    }

    Node *parent = area->get_parent();
    if (parent == nullptr || !parent->is_in_group(required_group)) {
        return false;
    }

    auto *unit = Object::cast_to<Unit>(parent);
    return unit != nullptr && !unit->is_dead();
}

void GameManager::on_scroll_triggered(Area2D *area, bool move_left) {
    if (match_director_.is_game_over()) {
        return;
    }

    if (!is_valid_scroll_trigger_unit(area, move_left ? "hostiles" : "friendlies")) {
        return;
    }

    const bool changed = move_left ? camera_scroll_controller_.retreat_target() : camera_scroll_controller_.advance_target();
    if (changed) {
        update_scroll_trigger_positions();
    }
}

void GameManager::add_friendly_unit(Unit *unit) {
    if (entity_container == nullptr || unit == nullptr) {
        return;
    }

    unit->connect("unit_died", callable_mp(this, &GameManager::on_friendly_died));
    entity_container->add_child(unit);
}

void GameManager::add_enemy_unit(Unit *unit) {
    if (entity_container == nullptr || unit == nullptr) {
        return;
    }

    unit->connect("unit_died", callable_mp(this, &GameManager::on_enemy_died));
    entity_container->add_child(unit);
}

void GameManager::refresh_resource_ui() {
    if (hud == nullptr) {
        return;
    }

    hud->update_core_resource(match_director_.get_core_resource());
    hud->update_card_affordability(match_director_.get_core_resource());
}

void GameManager::apply_match_update(const MatchUpdate &update) {
    for (const UnitSpawnRequest &friendly : update.friendly_spawn_requests) {
        add_friendly_unit(UnitFactory::materialize(friendly));
    }

    for (const UnitSpawnRequest &enemy : update.enemy_spawn_requests) {
        add_enemy_unit(UnitFactory::materialize(enemy));
    }

    if (hud != nullptr) {
        if (update.wave_changed.has_value()) {
            hud->update_wave(*update.wave_changed, match_director_.get_total_waves());
        }
        if (update.resources_changed) {
            refresh_resource_ui();
        }
        if (update.hearts_changed) {
            hud->update_hearts(update.hearts);
        }
        if (update.score_changed) {
            hud->update_score(update.score);
        }
        if (update.score_screen.has_value()) {
            hud->show_score_screen(*update.score_screen);
        }
    }

    if (update.match_finished && core_resource_timer != nullptr) {
        core_resource_timer->stop();
    }
}

void GameManager::on_enemy_died(Node *unit) {
    auto *hostile = Object::cast_to<Unit>(unit);
    const Vector2 death_position = hostile != nullptr ? hostile->get_global_position() : Vector2();
    const MatchUpdate update = match_director_.handle_enemy_died(hostile);
    if (entity_container != nullptr && update.bounty_awarded > 0) {
        vfx::spawn_bounty_energy_effect(entity_container, death_position, update.bounty_awarded);
    }
    apply_match_update(update);
}

void GameManager::on_friendly_died(Node * /*unit*/) {
    // Defender died — no special handling needed beyond removal
}

void GameManager::on_base_durability_changed(int current_hp, int /*max_hp*/) { apply_match_update(match_director_.handle_base_durability_changed(current_hp)); }

void GameManager::on_base_destroyed() {
    base_objective = nullptr;
    apply_match_update(match_director_.handle_base_destroyed());
}

void GameManager::on_core_resource_tick() { apply_match_update(match_director_.handle_core_resource_tick()); }

void GameManager::on_deploy_requested(const String &unit_type) { apply_match_update(match_director_.handle_deploy_request(unit_type)); }

void GameManager::on_score_screen_next_level(const String &level_id) {
    if (!match_director_.finalize_selected_upgrade()) {
        return;
    }

    match_director_.clear_pending_score_screen();
    SceneNavigator::go_to_level(get_tree(), level_id);
}

void GameManager::on_score_screen_retry(const String &level_id) {
    if (!match_director_.finalize_selected_upgrade()) {
        return;
    }

    match_director_.clear_pending_score_screen();
    SceneNavigator::go_to_level(get_tree(), level_id);
}

void GameManager::on_score_screen_main_menu() {
    if (!match_director_.finalize_selected_upgrade()) {
        return;
    }

    match_director_.clear_pending_score_screen();
    SceneNavigator::go_to_main_menu(get_tree());
}

void GameManager::on_score_screen_upgrade_selected(const String &upgrade_id) {
    if (!match_director_.select_upgrade(upgrade_id)) {
        return;
    }

    const ScoreScreenModel *score_screen = match_director_.get_pending_score_screen();
    if (score_screen != nullptr && hud != nullptr) {
        hud->show_score_screen(*score_screen);
    }
}

} // namespace defn
