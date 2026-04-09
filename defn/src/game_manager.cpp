#include "game_manager.h"
#include "collision_layers.h"
#include "game_background_builder.h"
#include "grid_manager.h"
#include "hud.h"
#include "match_session.h"
#include "pause_menu.h"
#include "progression_manager.h"
#include "progression_presentation.h"
#include "scene_navigator.h"
#include "unit.h"
#include "unit_factory.h"
#include "wave_manager.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr real_t HALF = 2.0F;

bool has_upgrade_option(const Array &available_upgrades, const String &upgrade_id) {
    // NOLINTNEXTLINE(readability-use-anyofallof)
    for (const Variant &card_variant : available_upgrades) {
        const Dictionary card = card_variant;
        if (String(card.get("id", "")) == upgrade_id) {
            return true;
        }
    }

    return false;
}

bool finalize_selected_upgrade(Dictionary &pending_score_screen_stats) {
    if (pending_score_screen_stats.is_empty()) {
        return true;
    }

    const Array available_upgrades = pending_score_screen_stats.get("available_upgrades", Array());
    if (available_upgrades.is_empty()) {
        return true;
    }

    const Dictionary selected_upgrade = pending_score_screen_stats.get("selected_upgrade", Dictionary());
    const String upgrade_id = selected_upgrade.get("id", "");
    const String level_id = pending_score_screen_stats.get("current_level_id", "");
    if (upgrade_id.is_empty() || level_id.is_empty()) {
        return false;
    }

    auto *progression = ProgressionManager::get_singleton();
    if (progression == nullptr) {
        return false;
    }

    if (!progression->claim_level_upgrade(level_id, upgrade_id)) {
        return false;
    }

    progression->save();
    return true;
}

} // namespace

GameManager::GameManager() = default;

void GameManager::_bind_methods() {}

void GameManager::_ready() {
    UtilityFunctions::print("GameManager: Initializing belt scroller game...");

    auto *progression = ProgressionManager::get_singleton();
    String level_id = progression->get_current_level_id();
    String level_path = vformat("res://data/levels/%s.json", level_id);

    // Load unit data from JSON
    unit_data_.load("res://data/unit_data.json", "res://data/unit_globals.json");

    if (auto *grid = GridManager::get_singleton()) {
        grid->configure(unit_data_.get_globals().gameplay_rules);
        camera_scroll_controller_.configure(grid->get_rules(), grid->get_world_width());
    }

    // Query progression for effective values
    const real_t bounty_multiplier = progression->get_effective_bounty_multiplier();
    const int energy_regen_rate = progression->get_effective_energy_regen();

    // Wave manager (load level first to get background path)
    wave_manager = memnew(WaveManager);
    wave_manager->set_name("WaveManager");
    add_child(wave_manager);

    wave_manager->set_unit_data(&unit_data_);
    wave_manager->load_level(level_path);

    // Setup camera and visual layers using background from level data
    String bg_path = wave_manager->get_background_path();
    if (bg_path.is_empty()) {
        bg_path = "res://assets/backgrounds/middle_east_ruin_tiling.png";
    }
    setup_background(bg_path);
    setup_camera();
    setup_scroll_trigger();

    // Entity container (y-sort so closer-to-bottom entities render in front)
    entity_container = memnew(Node2D);
    entity_container->set_name("EntityContainer");
    entity_container->set_y_sort_enabled(true);
    add_child(entity_container);

    const int base_resource = wave_manager->get_starting_core_resource();
    const int starting_core_resource = progression->get_effective_starting_energy(base_resource);
    const int initial_integrity = progression->get_effective_base_integrity(wave_manager->get_base_integrity());

    MatchConfig match_config{
        .starting_core_resource = starting_core_resource,
        .initial_integrity = initial_integrity,
        .bounty_multiplier = bounty_multiplier,
        .energy_regen_rate = energy_regen_rate,
    };
    match_session_.start(match_config);

    wave_manager->connect("enemy_spawned", callable_mp(this, &GameManager::on_enemy_spawned));
    wave_manager->connect("wave_changed", callable_mp(this, &GameManager::on_wave_changed));
    wave_manager->connect("all_spawns_complete", callable_mp(this, &GameManager::on_all_spawns_complete));

    // HUD
    hud = memnew(HUD);
    hud->set_name("HUD");
    add_child(hud);

    // Filter friendly units to only unlocked ones
    PackedStringArray unlocked_units = progression->get_unlocked_units();
    auto all_friendlies = unit_data_.get_friendly_units();
    std::vector<UnitConfig> available_friendlies;
    for (const auto &cfg : all_friendlies) {
        if (unlocked_units.has(cfg.name)) {
            available_friendlies.push_back(cfg);
        }
    }
    hud->set_friendly_units(available_friendlies);

    hud->update_core_resource(match_session_.get_core_resource());
    hud->update_wave(1, wave_manager->get_total_waves());
    hud->update_hearts(match_session_.get_base_integrity());
    hud->update_card_affordability(match_session_.get_core_resource());
    hud->update_score(0);
    hud->connect("deploy_requested", callable_mp(this, &GameManager::on_deploy_requested));
    hud->connect("score_screen_next_level", callable_mp(this, &GameManager::on_score_screen_next_level));
    hud->connect("score_screen_retry", callable_mp(this, &GameManager::on_score_screen_retry));
    hud->connect("score_screen_main_menu", callable_mp(this, &GameManager::on_score_screen_main_menu));
    hud->connect("score_screen_upgrade_selected", callable_mp(this, &GameManager::on_score_screen_upgrade_selected));

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

    // Start the game
    wave_manager->start();
}

void GameManager::_process(double delta) {
    if (match_session_.is_game_over()) {
        return;
    }

    update_camera_scroll(delta);
    check_victory();
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

void GameManager::setup_scroll_trigger() {
    scroll_trigger = memnew(Area2D);
    scroll_trigger->set_name("ScrollTrigger");
    scroll_trigger->set_collision_layer(CollisionLayers::NONE);
    scroll_trigger->set_collision_mask(CollisionLayers::SCROLL_TRIGGER_MASK);
    scroll_trigger->set_monitoring(true);
    scroll_trigger->set_monitorable(false);

    auto *shape_node = memnew(CollisionShape2D);
    Ref<RectangleShape2D> rect;
    rect.instantiate();
    // Tall vertical strip covering well beyond the belt area
    rect->set_size(Vector2(20.0, camera_scroll_controller_.get_trigger_height()));
    shape_node->set_shape(rect);
    scroll_trigger->add_child(shape_node);

    update_scroll_trigger_position();

    scroll_trigger->connect("area_entered", callable_mp(this, &GameManager::on_scroll_triggered));
    add_child(scroll_trigger);
}

void GameManager::update_scroll_trigger_position() {
    if (scroll_trigger != nullptr) {
        scroll_trigger->set_position(camera_scroll_controller_.get_trigger_position());
    }
}

void GameManager::on_scroll_triggered(Area2D *area) {
    if (match_session_.is_game_over()) {
        return;
    }

    // Verify the overlapping area belongs to a living friendly
    Node *parent = area->get_parent();
    if (!parent || !parent->is_in_group("friendlies")) {
        return;
    }
    auto *unit = Object::cast_to<Unit>(parent);
    if (!unit || unit->is_dead()) {
        return;
    }

    if (camera_scroll_controller_.advance_target()) {
        update_scroll_trigger_position();
    }
}

void GameManager::deploy_friendly(const String &unit_type) {
    auto cfg = unit_data_.get_unit(unit_type);
    if (!cfg) {
        return;
    }

    if (!match_session_.can_spend_energy(cfg->cost)) {
        return;
    }

    auto *grid = GridManager::get_singleton();
    match_session_.spend_energy(cfg->cost);

    const real_t spawn_x_pos = grid->deploy_x();
    const real_t spawn_y_pos = GridManager::random_belt_y();
    auto *unit = UnitFactory::create(*cfg, Vector2(spawn_x_pos, spawn_y_pos));

    unit->connect("unit_died", callable_mp(this, &GameManager::on_friendly_died));
    entity_container->add_child(unit);

    hud->update_core_resource(match_session_.get_core_resource());
    hud->update_card_affordability(match_session_.get_core_resource());
}

void GameManager::on_enemy_spawned(Node *enemy_node) {
    auto *enemy = Object::cast_to<Unit>(enemy_node);
    if (!enemy) {
        return;
    }

    enemy->connect("unit_died", callable_mp(this, &GameManager::on_enemy_died));
    enemy->connect("enemy_breached", callable_mp(this, &GameManager::on_enemy_breached));
    entity_container->add_child(enemy);
    match_session_.record_enemy_spawned();
}

void GameManager::on_wave_changed(int wave_number) {
    if (hud) {
        hud->update_wave(wave_number, wave_manager->get_total_waves());
    }
    UtilityFunctions::print(vformat("Wave %d started!", wave_number));
}

void GameManager::on_all_spawns_complete() { match_session_.mark_all_spawns_complete(); }

void GameManager::on_enemy_died(Node *unit) {
    auto *hostile = Object::cast_to<Unit>(unit);
    if (hostile && !hostile->is_queued_for_deletion()) {
        match_session_.record_enemy_died(hostile->get_bounty());
        hud->update_core_resource(match_session_.get_core_resource());
        hud->update_card_affordability(match_session_.get_core_resource());
        hud->update_score(match_session_.get_kill_score());
    }
}

void GameManager::on_friendly_died(Node * /*unit*/) {
    // Defender died — no special handling needed beyond removal
}

void GameManager::on_core_resource_tick() {
    match_session_.tick_energy();
    hud->update_core_resource(match_session_.get_core_resource());
    hud->update_card_affordability(match_session_.get_core_resource());
}

void GameManager::on_deploy_requested(const String &unit_type) {
    if (match_session_.is_game_over()) {
        return;
    }

    auto cfg = unit_data_.get_unit(unit_type);
    if (!cfg || !match_session_.can_spend_energy(cfg->cost)) {
        return;
    }

    deploy_friendly(unit_type);
}

void GameManager::on_enemy_breached() {
    const bool defeated = match_session_.record_enemy_breached();
    hud->update_hearts(match_session_.get_base_integrity());

    if (defeated) {
        end_game(false);
    }
}

void GameManager::check_victory() {
    if (match_session_.should_end_with_victory()) {
        end_game(true);
    }
}

void GameManager::end_game(bool victory) {
    if (!match_session_.finish_game()) {
        return;
    }

    core_resource_timer->stop();
    wave_manager->stop();

    auto *progression = ProgressionManager::get_singleton();
    String level_id = progression->get_current_level_id();
    int old_score = progression->get_total_score();

    const int level_score = match_session_.calculate_level_score(victory);

    // Update progression
    progression->add_score(level_score);
    Array available_upgrades;
    if (victory && progression->can_claim_level_upgrade(level_id)) {
        available_upgrades = progression->build_upgrade_draft_for_level(level_id);
    }
    if (victory) {
        progression->mark_level_completed(level_id, level_score);
    }
    int new_total = progression->get_total_score();
    progression->save();

    // Compute new unlocks
    PackedStringArray new_unlocks = ProgressionPresentation::describe_new_unlocks(*progression, old_score, new_total);

    // Determine next level
    String next_level_id;
    if (victory) {
        const auto &level_data = progression->get_level_unlock_data();
        for (size_t i = 0; i < level_data.size(); ++i) {
            if (level_data[i].level_id == level_id && i + 1 < level_data.size()) {
                String candidate = level_data[i + 1].level_id;
                if (progression->is_level_unlocked(candidate)) {
                    next_level_id = candidate;
                }
                break;
            }
        }
    }

    pending_score_screen_stats_ =
        match_session_.build_end_game_stats(victory, new_total, level_id, next_level_id, new_unlocks, available_upgrades, Dictionary());
    hud->show_score_screen(pending_score_screen_stats_);
}

void GameManager::on_score_screen_next_level(const String &level_id) {
    if (!finalize_selected_upgrade(pending_score_screen_stats_)) {
        return;
    }

    pending_score_screen_stats_.clear();
    SceneNavigator::go_to_level(get_tree(), level_id);
}

void GameManager::on_score_screen_retry(const String &level_id) {
    if (!finalize_selected_upgrade(pending_score_screen_stats_)) {
        return;
    }

    pending_score_screen_stats_.clear();
    SceneNavigator::go_to_level(get_tree(), level_id);
}

void GameManager::on_score_screen_main_menu() {
    if (!finalize_selected_upgrade(pending_score_screen_stats_)) {
        return;
    }

    pending_score_screen_stats_.clear();
    SceneNavigator::go_to_main_menu(get_tree());
}

void GameManager::on_score_screen_upgrade_selected(const String &upgrade_id) {
    if (upgrade_id.is_empty() || pending_score_screen_stats_.is_empty()) {
        return;
    }

    const Array available_upgrades = pending_score_screen_stats_.get("available_upgrades", Array());
    if (!has_upgrade_option(available_upgrades, upgrade_id)) {
        return;
    }

    auto *progression = ProgressionManager::get_singleton();
    if (progression == nullptr) {
        return;
    }

    pending_score_screen_stats_["selected_upgrade"] = progression->get_upgrade_card_view(upgrade_id);
    hud->show_score_screen(pending_score_screen_stats_);
}

} // namespace defn
