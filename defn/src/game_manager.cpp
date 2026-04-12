#include "game_manager.h"

#include "base_objective.h"
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

struct RewardOffer {
    Array available_upgrades;
    Dictionary reward_context;
};

std::optional<UnitConfig> get_upgraded_friendly_config(const ProgressionManager *progression, const UnitDataLoader &unit_data, const String &unit_type) {
    if (progression == nullptr) {
        return std::nullopt;
    }

    const auto base_config = unit_data.get_unit(unit_type);
    if (!base_config || base_config->side != UnitSide::FRIENDLY) {
        return std::nullopt;
    }

    return progression->get_effective_friendly_unit_config(*base_config);
}

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
    const String reward_source = pending_score_screen_stats.get("reward_source", "");
    const String reward_level_id = pending_score_screen_stats.get("reward_level_id", "");
    if (upgrade_id.is_empty() || reward_source.is_empty() || reward_level_id.is_empty()) {
        return false;
    }

    auto *progression = ProgressionManager::get_singleton();
    if (progression == nullptr) {
        return false;
    }

    bool claim_succeeded = false;
    if (reward_source == "first_clear") {
        claim_succeeded = progression->claim_level_upgrade(reward_level_id, upgrade_id);
    } else if (reward_source == "rescue") {
        claim_succeeded = progression->claim_rescue_draft(reward_level_id, upgrade_id);
    }

    if (!claim_succeeded) {
        return false;
    }

    progression->save();
    return true;
}

RewardOffer build_reward_offer(const ProgressionManager &progression, bool victory, const String &level_id, const String &frontier_level_id) {
    RewardOffer offer;

    if (victory && progression.can_claim_level_upgrade(level_id)) {
        offer.available_upgrades = progression.build_upgrade_draft_for_level(level_id);
        if (!offer.available_upgrades.is_empty()) {
            offer.reward_context["reward_source"] = "first_clear";
            offer.reward_context["reward_level_id"] = level_id;
        }
        return offer;
    }

    if (!victory && !frontier_level_id.is_empty() && progression.can_claim_rescue_draft(frontier_level_id)) {
        offer.available_upgrades = progression.build_rescue_draft_for_level(frontier_level_id);
        if (!offer.available_upgrades.is_empty()) {
            offer.reward_context["reward_source"] = "rescue";
            offer.reward_context["reward_level_id"] = frontier_level_id;
        }
    }

    return offer;
}

void finalize_reward_context(Dictionary &reward_context) {
    const String reward_source = reward_context.get("reward_source", "");
    const String reward_level_id = reward_context.get("reward_level_id", "");
    if (!reward_source.is_empty() && !reward_level_id.is_empty()) {
        reward_context["reward_title"] = ProgressionPresentation::format_reward_title(reward_source, reward_level_id);
        reward_context["reward_subtitle"] = ProgressionPresentation::format_reward_subtitle(reward_source, reward_level_id);
    }
}

String determine_next_level_id(const ProgressionManager &progression, bool victory, const String &level_id) {
    if (!victory) {
        return {};
    }

    const auto &level_data = progression.get_level_unlock_data();
    for (size_t i = 0; i < level_data.size(); ++i) {
        if (level_data[i].level_id == level_id && i + 1 < level_data.size()) {
            const String candidate = level_data[i + 1].level_id;
            return progression.is_level_unlocked(candidate) ? candidate : String();
        }
    }

    return {};
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
    available_friendlies.reserve(all_friendlies.size());
    for (const auto &cfg : all_friendlies) {
        if (unlocked_units.has(cfg.name)) {
            available_friendlies.push_back(progression->get_effective_friendly_unit_config(cfg));
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

void GameManager::setup_base_objective() {
    if (entity_container == nullptr) {
        return;
    }

    base_objective = memnew(BaseObjective);
    base_objective->set_name("BaseObjective");
    base_objective->connect("durability_changed", callable_mp(this, &GameManager::on_base_durability_changed));
    base_objective->connect("objective_destroyed", callable_mp(this, &GameManager::on_base_destroyed));
    entity_container->add_child(base_objective);
    base_objective->configure(match_session_.get_base_max_health(), get_base_objective_position());
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
    if (match_session_.is_game_over()) {
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

void GameManager::deploy_friendly(const String &unit_type) {
    auto *progression = ProgressionManager::get_singleton();
    const auto cfg = get_upgraded_friendly_config(progression, unit_data_, unit_type);
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

void GameManager::on_base_durability_changed(int current_hp, int /*max_hp*/) {
    match_session_.set_base_health(current_hp);
    if (hud != nullptr) {
        hud->update_hearts(match_session_.get_base_integrity());
    }
}

void GameManager::on_base_destroyed() {
    base_objective = nullptr;
    match_session_.set_base_health(0);
    if (hud != nullptr) {
        hud->update_hearts(match_session_.get_base_integrity());
    }

    end_game(false);
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

    auto *progression = ProgressionManager::get_singleton();
    const auto cfg = get_upgraded_friendly_config(progression, unit_data_, unit_type);
    if (!cfg || !match_session_.can_spend_energy(cfg->cost)) {
        return;
    }

    deploy_friendly(unit_type);
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
    const String frontier_level_id = progression->get_frontier_level_id();

    const int level_score = match_session_.calculate_level_score(victory);

    progression->add_score(level_score);
    RewardOffer reward_offer = build_reward_offer(*progression, victory, level_id, frontier_level_id);
    finalize_reward_context(reward_offer.reward_context);

    if (victory) {
        progression->mark_level_completed(level_id, level_score);
    }
    int new_total = progression->get_total_score();
    progression->save();

    PackedStringArray new_unlocks = ProgressionPresentation::describe_new_unlocks(*progression, victory, level_id);
    const String next_level_id = determine_next_level_id(*progression, victory, level_id);

    pending_score_screen_stats_ = match_session_.build_end_game_stats(victory, new_total, level_id, next_level_id, new_unlocks, reward_offer.available_upgrades,
                                                                      Dictionary(), reward_offer.reward_context);
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
