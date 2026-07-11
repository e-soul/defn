#include "game_manager.h"

#include "base_objective.h"
#include "base_objective_factory.h"
#include "bounty_energy_effect.h"
#include "collision_layers.h"
#include "data_paths.h"
#include "game_background_builder.h"
#include "godot_string.h"
#include "grid_manager.h"
#include "hud.h"
#include "level_loader.h"
#include "match_director.h"
#include "menu_flow_use_case.h"
#include "pause_menu.h"
#include "progression_manager.h"
#include "scene_navigator.h"
#include "score_screen_models.h"
#include "unit.h"
#include "unit_factory.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr real_t HALF = 2.0F;

float linear_to_db(float linear) {
    const float clamped_linear = std::clamp(linear, 0.0001F, 1.0F);
    return 20.0F * std::log10(clamped_linear);
}

UpgradeCardViewModel to_upgrade_card_view_model(const MatchUpgradeOption &option) {
    return {
        .id = option.id,
        .name = option.name,
        .description = option.description,
        .emoji = option.emoji,
        .category = option.category,
        .owned_count = option.owned_count,
    };
}

std::vector<UpgradeCardViewModel> to_upgrade_card_view_models(const std::vector<MatchUpgradeOption> &options) {
    std::vector<UpgradeCardViewModel> result;
    result.reserve(options.size());
    for (const auto &option : options) {
        result.push_back(to_upgrade_card_view_model(option));
    }
    return result;
}

ScoreScreenRewardModel to_score_screen_reward_model(const MatchRewardOptions &options) {
    ScoreScreenRewardModel reward;
    reward.source = options.source;
    reward.level_id = options.level_id;
    reward.title = options.title;
    reward.subtitle = options.subtitle;
    reward.available_upgrades = to_upgrade_card_view_models(options.available_upgrades);
    if (options.selected_upgrade.has_value()) {
        reward.selected_upgrade = to_upgrade_card_view_model(*options.selected_upgrade);
    }
    return reward;
}

ScoreScreenModel to_score_screen_model(const MatchEnded &match_end) {
    const MatchSummaryModel &summary = match_end.summary_model;
    ScoreScreenModel model;
    model.victory = summary.victory;
    model.enemies_killed = summary.enemies_killed;
    model.kill_score = summary.kill_score;
    model.hearts_remaining = summary.hearts_remaining;
    model.hearts_total = summary.hearts_total;
    model.integrity_bonus = summary.integrity_bonus;
    model.completion_bonus = summary.completion_bonus;
    model.level_score = summary.level_score;
    model.new_total_score = summary.new_total_score;
    model.current_level_id = summary.current_level_id;
    model.next_level_id = summary.next_level_id;
    for (const auto &new_unlock : summary.new_unlocks) {
        model.new_unlocks.push_back(new_unlock);
    }
    model.reward = to_score_screen_reward_model(match_end.reward_options);
    model.owned_upgrades = to_upgrade_card_view_models(match_end.owned_upgrades);
    return model;
}

MenuFlowUseCase make_menu_flow_use_case() { return MenuFlowUseCase(CampaignService::get_singleton()); }

void navigate_if_requested(SceneTree *tree, const MenuFlowResult &result) {
    if (result.navigation.has_value()) {
        SceneNavigator::navigate(tree, *result.navigation);
    }
}

} // namespace

GameManager::GameManager() = default;

void GameManager::_bind_methods() {}

void GameManager::_ready() {
    UtilityFunctions::print("GameManager: Initializing belt scroller game...");

    auto *progression = CampaignService::get_singleton();
    String level_id = progression->get_current_level_id_godot();
    const String level_path = DataPaths::level_definition(level_id);

    // Load unit data from JSON
    unit_data_.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS);

    if (auto *grid = GridManager::get_singleton()) {
        grid->configure(unit_data_.get_globals().gameplay_rules);
        camera_scroll_controller_.configure(grid->get_rules(), grid->get_world_width());
    }

    match_director_.configure(progression, &unit_data_, GridManager::get_singleton());
    const auto loaded_level = LevelLoader::load(level_path);
    if (!loaded_level) {
        UtilityFunctions::printerr("GameManager: Failed to load level: ", level_path);
        return;
    }
    match_director_.load_level_definition(*loaded_level, to_std_string(level_id));
    match_director_.begin_match();

    // Setup camera and visual layers using background from level data
    String bg_path = to_godot_string(match_director_.get_background_path());
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
    hud->set_level(match_director_.get_level_number(), to_godot_string(match_director_.get_level_name()));
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
    setup_match_result_cutscene_timer();
    setup_match_result_reveal_timer();

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

    base_objective = BaseObjectiveFactory::create(match_director_.get_base_max_health(), get_base_objective_position(), base_visual_config);
    base_objective->connect("durability_changed", callable_mp(this, &GameManager::on_base_durability_changed));
    base_objective->connect("objective_destroyed", callable_mp(this, &GameManager::on_base_destroyed));
    entity_container->add_child(base_objective);
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
    rect->set_size(godot::Vector2(20.0, camera_scroll_controller_.get_trigger_height()));
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

godot::Vector2 GameManager::get_base_objective_position() {
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

Unit *GameManager::materialize_spawn_intent(const SpawnUnitIntent &intent) {
    auto config = unit_data_.get_unit(intent.unit_id);
    if (!config) {
        UtilityFunctions::printerr("GameManager: Missing unit config for spawn intent: ", to_godot_string(intent.unit_id));
        return nullptr;
    }

    if (intent.side == MatchUnitSide::Friendly) {
        if (auto *progression = CampaignService::get_singleton()) {
            *config = progression->get_effective_friendly_unit_config(*config);
        }
    }

    return UnitFactory::materialize(intent, *config);
}

void GameManager::refresh_resource_ui(int energy) {
    if (hud == nullptr) {
        return;
    }

    hud->update_core_resource(energy);
    hud->update_card_affordability(energy);
}

void GameManager::setup_match_result_cutscene_timer() {
    if (match_result_cutscene_timer_ != nullptr) {
        return;
    }

    match_result_cutscene_timer_ = memnew(Timer);
    match_result_cutscene_timer_->set_name("MatchResultCutsceneTimer");
    match_result_cutscene_timer_->set_one_shot(true);
    match_result_cutscene_timer_->connect("timeout", callable_mp(this, &GameManager::finish_match_result_cutscene));
    add_child(match_result_cutscene_timer_);
}

void GameManager::setup_match_result_reveal_timer() {
    if (match_result_reveal_timer_ != nullptr) {
        return;
    }

    match_result_reveal_timer_ = memnew(Timer);
    match_result_reveal_timer_->set_name("MatchResultRevealTimer");
    match_result_reveal_timer_->set_one_shot(true);
    match_result_reveal_timer_->connect("timeout", callable_mp(this, &GameManager::reveal_match_result_cutscene));
    add_child(match_result_reveal_timer_);
}

void GameManager::apply_match_update(const MatchUpdate &update) {
    for (const SpawnUnitIntent &intent : update.spawn_unit_intents) {
        Unit *unit = materialize_spawn_intent(intent);
        if (intent.side == MatchUnitSide::Friendly) {
            add_friendly_unit(unit);
        } else {
            add_enemy_unit(unit);
        }
    }

    if (hud != nullptr) {
        if (update.wave_changed.has_value()) {
            hud->update_wave(update.wave_changed->current_wave, update.wave_changed->total_waves);
        }
        if (update.resource_changed.has_value()) {
            refresh_resource_ui(update.resource_changed->energy);
        }
        if (update.integrity_changed.has_value()) {
            hud->update_hearts(update.integrity_changed->hearts);
        }
        if (update.score_changed.has_value()) {
            hud->update_score(update.score_changed->kill_score);
        }
    }

    if (update.match_ended.has_value()) {
        start_match_result_cutscene(*update.match_ended);
    }

    if (update.match_ended.has_value() && core_resource_timer != nullptr) {
        core_resource_timer->stop();
    }
}

void GameManager::start_match_result_cutscene(const MatchEnded &match_end) {
    if (match_result_cutscene_active_) {
        return;
    }

    pending_score_screen_model_ = to_score_screen_model(match_end);
    const MatchResultCutsceneModel cutscene_model = MatchResultCutscenePresenter::build(match_end.victory);
    pending_match_result_cutscene_model_ = cutscene_model;
    match_result_cutscene_active_ = true;

    if (core_resource_timer != nullptr) {
        core_resource_timer->stop();
    }

    if (cutscene_model.shake_base) {
        play_cutscene_sfx(cutscene_model.base_destroyed_sfx_path);
        if (base_objective != nullptr && !base_objective->is_queued_for_deletion()) {
            base_objective->play_destroyed_shake();
        }
    }

    freeze_match_result_units(cutscene_model.celebrant_side, cutscene_model.pre_reveal_animation_name);

    if (cutscene_model.reveal_delay_seconds > 0.0) {
        setup_match_result_reveal_timer();
        match_result_reveal_timer_->set_wait_time(cutscene_model.reveal_delay_seconds);
        match_result_reveal_timer_->start();
        return;
    }

    reveal_match_result_cutscene();
}

void GameManager::reveal_match_result_cutscene() {
    if (!pending_match_result_cutscene_model_.has_value()) {
        return;
    }

    const MatchResultCutsceneModel &cutscene_model = *pending_match_result_cutscene_model_;
    freeze_match_result_units(cutscene_model.celebrant_side, cutscene_model.reveal_animation_name);
    play_cutscene_sfx(cutscene_model.primary_sfx_path);
    if (hud != nullptr) {
        hud->show_match_result_banner(cutscene_model);
    }

    setup_match_result_cutscene_timer();
    match_result_cutscene_timer_->set_wait_time(cutscene_model.duration_seconds);
    match_result_cutscene_timer_->start();
}

void GameManager::finish_match_result_cutscene() {
    if (!pending_score_screen_model_.has_value()) {
        match_result_cutscene_active_ = false;
        pending_match_result_cutscene_model_.reset();
        return;
    }

    match_result_cutscene_active_ = false;
    pending_match_result_cutscene_model_.reset();
    if (hud != nullptr) {
        hud->hide_match_result_banner();
        hud->show_score_screen(*pending_score_screen_model_);
    }
    pending_score_screen_model_.reset();
}

void GameManager::freeze_match_result_units(MatchResultCelebrantSide side, const std::string &animation_name) {
    if (entity_container == nullptr || animation_name.empty()) {
        return;
    }

    const StringName animation = StringName(to_godot_string(animation_name));
    const UnitSide unit_side = side == MatchResultCelebrantSide::Friendly ? UnitSide::FRIENDLY : UnitSide::HOSTILE;
    const int child_count = entity_container->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index) {
        auto *unit = Object::cast_to<Unit>(entity_container->get_child(child_index));
        if (unit != nullptr && unit->get_side() == unit_side) {
            unit->freeze_for_match_result(animation);
        }
    }
}

void GameManager::play_cutscene_sfx(const std::string &path, float volume_linear) {
    if (path.empty()) {
        return;
    }

    auto *loader = ResourceLoader::get_singleton();
    if (loader == nullptr) {
        return;
    }

    Ref<AudioStream> stream = loader->load(to_godot_string(path));
    if (!stream.is_valid()) {
        UtilityFunctions::printerr("GameManager: Failed to load cutscene SFX: ", to_godot_string(path));
        return;
    }

    auto *player = memnew(AudioStreamPlayer);
    player->set_name("MatchResultSfxPlayer");
    player->set_stream(stream);
    player->set_volume_db(linear_to_db(volume_linear));
    Node *player_node = player;
    player->connect("finished", callable_mp(player_node, &Node::queue_free));
    add_child(player);
    player->play();
}

void GameManager::on_enemy_died(Node *unit) {
    auto *hostile = Object::cast_to<Unit>(unit);
    const godot::Vector2 death_position = hostile != nullptr ? hostile->get_global_position() : godot::Vector2();
    const MatchUpdate update =
        hostile != nullptr && !hostile->is_queued_for_deletion() ? match_director_.handle_enemy_defeated({.bounty = hostile->get_bounty()}) : MatchUpdate{};
    if (entity_container != nullptr && update.score_changed.has_value() && update.score_changed->bounty_awarded > 0) {
        vfx::spawn_bounty_energy_effect(entity_container, death_position, update.score_changed->bounty_awarded);
    }
    apply_match_update(update);
}

void GameManager::on_friendly_died(Node * /*unit*/) {
    // Defender died — no special handling needed beyond removal
}

void GameManager::on_base_durability_changed(int current_hp, int /*max_hp*/) { apply_match_update(match_director_.handle_base_durability_changed(current_hp)); }

void GameManager::on_base_destroyed() { apply_match_update(match_director_.handle_base_destroyed()); }

void GameManager::on_core_resource_tick() { apply_match_update(match_director_.handle_core_resource_tick()); }

void GameManager::on_deploy_requested(const String &unit_type) {
    if (match_result_cutscene_active_ || match_director_.is_game_over()) {
        return;
    }

    apply_match_update(match_director_.handle_deploy_request(to_std_string(unit_type)));
}

void GameManager::on_score_screen_next_level(const String &level_id) {
    if (!match_director_.finalize_selected_upgrade()) {
        return;
    }

    const MenuFlowResult result = make_menu_flow_use_case().select_level(to_std_string(level_id));
    if (!result.navigation.has_value()) {
        return;
    }

    match_director_.clear_pending_match_end();
    navigate_if_requested(get_tree(), result);
}

void GameManager::on_score_screen_retry(const String &level_id) {
    if (!match_director_.finalize_selected_upgrade()) {
        return;
    }

    const MenuFlowResult result = make_menu_flow_use_case().select_level(to_std_string(level_id));
    if (!result.navigation.has_value()) {
        return;
    }

    match_director_.clear_pending_match_end();
    navigate_if_requested(get_tree(), result);
}

void GameManager::on_score_screen_main_menu() {
    if (!match_director_.finalize_selected_upgrade()) {
        return;
    }

    match_director_.clear_pending_match_end();
    navigate_if_requested(get_tree(), MenuFlowUseCase::request_main_menu());
}

void GameManager::on_score_screen_upgrade_selected(const String &upgrade_id) {
    if (!match_director_.select_upgrade(to_std_string(upgrade_id))) {
        return;
    }

    const MatchEnded *match_end = match_director_.get_pending_match_end();
    if (match_end != nullptr && hud != nullptr) {
        hud->show_score_screen(to_score_screen_model(*match_end));
    }
}

} // namespace defn
