// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "match_presentation.h"
#include "data_paths.h"
#include "deploy_tutorial_prompt.h"
#include "endless_schedule_loader.h"
#include "game_manager.h"
#include "godot_string.h"
#include "hud.h"
#include "level_loader.h"
#include "match_resource_plan.h"
#include "match_result_cutscene_view_model.h"
#include "music_playlist_loader.h"
#include "progression_manager.h"
#include "scene_navigator.h"
#include "ui_screen_scaffold.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include "unit_control_config_loader.h"
#include "unit_data.h"
#include "unit_selection_controller.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {
namespace {
std::vector<std::string> presentation_resource_paths(const PreparedMatch &match, const CampaignService &progression) {
    std::vector<std::string> presentation_paths;
    presentation_paths.emplace_back(DeployTutorialPrompt::SOUND_PATH);
    for (const bool victory : {false, true}) {
        const auto result = MatchResultCutscenePresenter::build(victory);
        presentation_paths.push_back(result.primary_sfx_path);
        presentation_paths.push_back(result.base_destroyed_sfx_path);
    }
    const auto &theme = UiThemeProvider::data();
    for (const auto *sound : {&theme.sfx.hover, &theme.sfx.click, &theme.sfx.deploy_card}) {
        presentation_paths.push_back(sound->path);
    }
    for (const auto *name : {"energy", "integrity", "wave", "score", "squad", "level"}) {
        if (const auto *icon = theme.find_icon(name); icon != nullptr) {
            presentation_paths.push_back(icon->mark);
        }
    }
    if (std::ranges::any_of(match.selected_units, [](const UnitConfig &unit) { return unit.side == UnitSide::FRIENDLY && unit.name != "base"; })) {
        presentation_paths.emplace_back("res://assets/sfx/field_promotion_kalimba.wav");
        if (const auto *icon = theme.find_icon("promotion_star"); icon != nullptr) {
            presentation_paths.push_back(icon->mark);
        }
    }
    for (const auto &icon_key : progression.match_upgrade_icon_keys(match.level_id, match.endless.has_value())) {
        const auto *icon = theme.find_icon(icon_key.empty() ? "generic" : icon_key);
        if (icon != nullptr) {
            presentation_paths.push_back(icon->mark);
        }
    }
    if (match.music.has_value()) {
        presentation_paths.insert(presentation_paths.end(), match.music->tracks.begin(), match.music->tracks.end());
    }
    return presentation_paths;
}
std::pair<float, float> protected_world_band(const UnitDataLoader &content, const std::vector<UnitConfig> &selected) {
    const auto &rules = content.get_globals().gameplay_rules;
    float top = rules.belt_top_y;
    float bottom = rules.belt_bottom_y;
    for (const auto &unit : selected) {
        if (unit.role == UnitRole::STRUCTURE) {
            continue;
        }
        top = std::min(top, rules.belt_top_y + ((unit.health_bar_offset.y - 24.0F) * unit.scale));
        for (const auto &[name, animation] : unit.animations) {
            const auto path = to_godot_string(animation.path_template);
            const godot::Ref<godot::Texture2D> texture = godot::ResourceLoader::get_singleton()->load(path.contains("%") ? godot::vformat(path, 0) : path);
            if (texture.is_null()) {
                continue;
            }
            const float half_height = static_cast<float>(texture->get_height()) * 0.5F;
            top = std::min(top, rules.belt_top_y + ((animation.offset.y - half_height) * unit.scale));
            bottom = std::max(bottom, rules.belt_bottom_y + ((animation.offset.y + half_height) * unit.scale));
        }
    }
    return {std::clamp(top / rules.viewport_height, 0.0F, 1.0F), std::clamp(bottom / rules.viewport_height, 0.0F, 1.0F)};
}
godot::Rect2 protected_base_bounds(godot::Node2D *base) {
    if (auto *sprite = godot::Object::cast_to<godot::Sprite2D>(base->get_node_or_null("TowerSprite")); sprite != nullptr) {
        return sprite->get_global_transform().xform(sprite->get_rect());
    }
    return {};
}
} // namespace

void MatchPresentation::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("is_match_ready"), &MatchPresentation::is_match_ready);
    godot::ClassDB::bind_method(godot::D_METHOD("has_loading_failed"), &MatchPresentation::has_loading_failed);
    ADD_SIGNAL(godot::MethodInfo("match_ready"));
}

void MatchPresentation::_ready() {
    get_tree()->set_pause(false);
    ResponsiveUiRoot::_ready();
    threaded_loading_ = !godot::OS::get_singleton()->has_feature("web") && godot::DisplayServer::get_singleton()->get_name() != "headless";
    godot::RenderingServer::get_singleton()->connect("frame_post_draw", callable_mp(this, &MatchPresentation::frame_drawn));
    build_loading_overlay();
}

void MatchPresentation::frame_drawn() { frame_drawn_ = true; }

void MatchPresentation::build_loading_overlay() {
    const auto screen =
        build_screen(this, {.title = "Preparing deployment", .show_backdrop = true, .panelled_body = true, .scrollable_body = false, .fit_content = true});
    loading_overlay_ = screen.root;
    loading_overlay_->set_name("LoadingOverlay");
    if (auto *backdrop = godot::Object::cast_to<godot::ColorRect>(loading_overlay_->get_node_or_null("ScreenBackdrop")); backdrop != nullptr) {
        auto color = UiThemeProvider::color("backdrop");
        color.a = 1;
        backdrop->set_color(color);
    }
    loading_overlay_->set_z_index(200);
    loading_overlay_->set_mouse_filter(MOUSE_FILTER_STOP);
    loading_spinner_ = make_label("|", "screen_display");
    loading_spinner_->set_name("LoadingSpinner");
    loading_spinner_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    set_state_tint(loading_spinner_, "accent");
    screen.body->add_child(loading_spinner_);
    loading_status_ = make_label("Preparing operation...", "tagline");
    loading_status_->set_name("LoadingStatus");
    loading_status_->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);
    loading_status_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    screen.body->add_child(loading_status_);
    loading_actions_ = screen.footer;
    loading_actions_->set_alignment(godot::FlowContainer::ALIGNMENT_CENTER);
    loading_actions_->add_child(make_button("Retry", "secondary", callable_mp(this, &MatchPresentation::retry_loading)));
    loading_actions_->add_child(make_button("Main Menu", "secondary", callable_mp(this, &MatchPresentation::return_to_menu)));
    loading_actions_->hide();
    notify_content(loading_overlay_);
}

void MatchPresentation::_process(double delta) {
    ResponsiveUiRoot::_process(delta);
    if (loading_state_ == LoadingState::Ready || loading_state_ == LoadingState::Failed) {
        return;
    }
    spinner_elapsed_ += delta;
    static constexpr std::array SPINNER = {"|", "/", "-", "\\"};
    loading_spinner_->set_text(SPINNER[static_cast<int>(spinner_elapsed_ * 8) % 4]);
    ++loading_frames_;
    // Headless tests have no presentation fence; normal launches must display the overlay first.
    const bool headless = godot::DisplayServer::get_singleton()->get_name() == "headless";
    switch (loading_state_) {
    case LoadingState::ShowingOverlay:
        if (frame_drawn_ || (headless && loading_frames_ > 1)) {
            loading_state_ = LoadingState::Preparing;
        }
        break;
    case LoadingState::Preparing:
        prepare_content();
        break;
    case LoadingState::LoadingResources:
        load_resources();
        break;
    case LoadingState::BuildingAnimations:
        if (animation_index_ < selected_units_.size()) {
            resources_->prepare_unit(selected_units_[animation_index_++]);
        } else {
            build_battlefield();
        }
        break;
    case LoadingState::BuildingMatch:
        if (game_->build_next_stage()) {
            if (!game_->construction_succeeded()) {
                fail_loading("The operation could not be built.");
                return;
            }
            loading_status_->set_text("Preparing battlefield...");
            loading_state_ = LoadingState::Rendering;
            frame_drawn_ = false;
        }
        break;
    case LoadingState::Rendering:
        if (headless) {
            render_index_ = resource_paths_.size();
        }
        if (frame_drawn_ || headless) {
            render_resources();
        }
        break;
    case LoadingState::Ready:
    case LoadingState::Failed:
        break;
    }
}

void MatchPresentation::prepare_content() {
    auto *progression = CampaignService::get_singleton();
    PreparedMatch match;
    if (progression == nullptr || !match.units.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS)) {
        fail_loading("Unit definitions could not be read.");
        return;
    }
    match.level_id = progression->get_current_level_id();
    if (progression->get_match_mode() == MatchMode::ENDLESS) {
        const auto endless = EndlessScheduleLoader::load(DataPaths::ENDLESS_DATA);
        if (!endless.has_value()) {
            fail_loading("The operation could not be read.");
            return;
        }
        match.level = endless->level;
        match.endless = endless->schedule;
        match.level_id = "endless";
    } else {
        const auto level = LevelLoader::load(DataPaths::level_definition(to_godot_string(match.level_id)));
        if (!level.has_value()) {
            fail_loading("The operation could not be read.");
            return;
        }
        match.level = *level;
    }
    const auto controls = UnitControlConfigLoader::load(DataPaths::UNIT_CONTROL);
    if (!controls.has_value()) {
        fail_loading("Unit controls could not be read.");
        return;
    }
    match.controls = *controls;
    match.music = MusicPlaylistLoader::load(DataPaths::MUSIC_PLAYLIST_DATA);
    if (!match.music.has_value()) {
        fail_loading("The music playlist could not be read.");
        return;
    }
    if (!select_units(match, *progression)) {
        return;
    }
    const auto presentation_paths = presentation_resource_paths(match, *progression);
    resource_paths_ = match_resource_paths(match.level, match.selected_units, presentation_paths);
    selected_units_ = match.selected_units;
    const auto &rules = match.units.get_globals().gameplay_rules;
    world_size_ = {.width = rules.viewport_width, .height = rules.viewport_height};
    if (std::abs((world_size_.width / world_size_.height) - (16.0F / 9.0F)) > 0.001F) {
        fail_loading("The battlefield dimensions are invalid.");
        return;
    }
    loading_status_->set_text("Loading " + to_godot_string(match.level.name) + "...");
    godot::UtilityFunctions::print("MatchPresentation: preparing ", to_godot_string(match.level_id), ": ", static_cast<int64_t>(selected_units_.size()),
                                   " unit types, ", static_cast<int64_t>(resource_paths_.size()), " resources");
    resources_ = std::make_shared<MatchResources>();
    prepared_ = std::move(match);
    loading_state_ = LoadingState::LoadingResources;
}

bool MatchPresentation::select_units(PreparedMatch &match, CampaignService &progression) {
    const auto unlocked = progression.get_unlocked_units();
    std::vector<std::string> friendlies;
    for (const auto &unit : match.units.get_friendly_units()) {
        if (unit.name != "base" && std::ranges::find(unlocked, unit.name) != unlocked.end()) {
            friendlies.push_back(unit.name);
        }
    }
    const auto ids = match_unit_ids(match.level, friendlies, match.endless.has_value() ? &*match.endless : nullptr);
    for (const auto &unit_id : ids) {
        auto unit = match.units.get_unit(unit_id);
        if (!unit.has_value()) {
            fail_loading("An operation unit is missing: " + to_godot_string(unit_id));
            return false;
        }
        if (unit->side == UnitSide::FRIENDLY && unit->name != "base") {
            *unit = progression.get_effective_friendly_unit_config(*unit);
        }
        match.selected_units.push_back(*unit);
    }
    return true;
}

void MatchPresentation::load_resources() {
    if (threaded_loading_) {
        load_threaded_resources();
    } else {
        load_incremental_resources();
    }
    if (loading_state_ != LoadingState::Failed && loaded_ == resource_paths_.size()) {
        loading_status_->set_text("Preparing units...");
        loading_state_ = LoadingState::BuildingAnimations;
    }
}

void MatchPresentation::load_threaded_resources() {
    auto *loader = godot::ResourceLoader::get_singleton();
    const auto deadline = godot::Time::get_singleton()->get_ticks_usec() + 2000;
    while (requested_ < resource_paths_.size() && pending_paths_.size() < 8) {
        const auto &path = resource_paths_[requested_++];
        const auto error = loader->load_threaded_request(to_godot_string(path), "", false, godot::ResourceLoader::CACHE_MODE_REUSE);
        if (error != godot::OK) {
            fail_resource(to_godot_string(path));
            return;
        }
        pending_paths_.push_back(path);
    }
    for (std::size_t index = 0; index < pending_paths_.size();) {
        const auto path = to_godot_string(pending_paths_[index]);
        const auto status = loader->load_threaded_get_status(path);
        if (status == godot::ResourceLoader::THREAD_LOAD_FAILED || status == godot::ResourceLoader::THREAD_LOAD_INVALID_RESOURCE) {
            fail_resource(path);
            return;
        }
        if (status == godot::ResourceLoader::THREAD_LOAD_LOADED) {
            const godot::Ref<godot::Resource> resource = loader->load_threaded_get(path);
            if (resource.is_null()) {
                fail_resource(path);
                return;
            }
            resources_->retain(pending_paths_[index], resource);
            pending_paths_.erase(pending_paths_.begin() + static_cast<std::ptrdiff_t>(index));
            ++loaded_;
        } else {
            ++index;
        }
        if (godot::Time::get_singleton()->get_ticks_usec() >= deadline) {
            break;
        }
    }
}

void MatchPresentation::load_incremental_resources() {
    auto *loader = godot::ResourceLoader::get_singleton();
    const auto deadline = godot::Time::get_singleton()->get_ticks_usec() + 2000;
    while (requested_ < resource_paths_.size()) {
        const auto &path = resource_paths_[requested_++];
        const godot::Ref<godot::Resource> resource = loader->load(to_godot_string(path));
        if (resource.is_null()) {
            fail_resource(to_godot_string(path));
            return;
        }
        resources_->retain(path, resource);
        ++loaded_;
        if (godot::Time::get_singleton()->get_ticks_usec() >= deadline) {
            break;
        }
    }
}

void MatchPresentation::build_battlefield() {
    if (!prepared_.has_value()) {
        fail_loading("Prepared operation is unavailable.");
        return;
    }
    auto *backdrop = memnew(godot::ColorRect);
    backdrop->set_color(UiThemeProvider::color("backdrop"));
    backdrop->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    backdrop->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(backdrop);
    host_ = memnew(godot::SubViewportContainer);
    host_->set_name("BattlefieldHost");
    host_->set_stretch(true);
    host_->set_mouse_filter(MOUSE_FILTER_STOP);
    add_child(host_);
    world_ = memnew(godot::SubViewport);
    world_->set_name("Battlefield");
    world_->set_disable_3d(true);
    world_->set_as_audio_listener_2d(true);
    world_->set_size_2d_override({static_cast<int>(world_size_.width), static_cast<int>(world_size_.height)});
    world_->set_size_2d_override_stretch(true);
    host_->add_child(world_);
    ui_ = memnew(godot::Control);
    ui_->set_name("FullScreenUI");
    ui_->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    ui_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(ui_);
    apply_layout();
    auto *game = memnew(GameManager);
    game->set_name("GameManager");
    game->configure_presentation(
        ui_, prepared_->units,
        [this](HUD *hud, const UnitDataLoader &content, godot::Node2D *base, UnitSelectionController *selection) {
            configure_hud(hud, content, base, selection);
        },
        [this] { invalidate_layout(); });
    game_ = game;
    game_->configure_prepared_match(std::move(*prepared_), resources_);
    prepared_.reset();
    world_->add_child(game_);
    move_child(loading_overlay_, get_child_count() - 1);
    loading_state_ = LoadingState::BuildingMatch;
}
void MatchPresentation::render_resources() {
    if (warmup_ != nullptr) {
        world_->remove_child(warmup_);
        warmup_->queue_free();
        warmup_ = nullptr;
    }
    if (render_index_ == resource_paths_.size()) {
        loading_overlay_->hide();
        game_->start_match();
        loading_state_ = LoadingState::Ready;
        emit_signal("match_ready");
        return;
    }
    warmup_ = memnew(godot::Node2D);
    warmup_->set_name("ResourceWarmup");
    world_->add_child(warmup_);
    // Draw the real textures behind the opaque loading screen; hidden sprites would skip renderer preparation.
    int count = 0;
    while (render_index_ < resource_paths_.size() && count < 16) {
        const godot::Ref<godot::Texture2D> texture = resources_->get(resource_paths_[render_index_++]);
        if (texture.is_null()) {
            continue;
        }
        auto *sprite = memnew(godot::Sprite2D);
        sprite->set_texture(texture);
        const float scale = 48.0F / static_cast<float>(std::max({texture->get_width(), texture->get_height(), 1}));
        sprite->set_scale({scale, scale});
        // Camera starts at the base, so draw inside its view, rather than at an offscreen world origin.
        const int row = count / 8;
        sprite->set_position({static_cast<float>((count % 8) * 60), 500.0F + static_cast<float>(row * 60)});
        warmup_->add_child(sprite);
        ++count;
    }
    frame_drawn_ = false;
}

void MatchPresentation::fail_resource(const godot::String &path) {
    godot::UtilityFunctions::printerr("MatchPresentation: failed resource ", path);
    fail_loading("Operation resources could not be loaded. Please retry.");
}

void MatchPresentation::fail_loading(const godot::String &message) {
    godot::UtilityFunctions::printerr("MatchPresentation: ", message);
    loading_state_ = LoadingState::Failed;
    loading_status_->set_text(message);
    loading_spinner_->set_text("!");
    loading_actions_->show();
    // A construction failure keeps any mounted UI inert behind the overlay.
    if (game_ != nullptr) {
        game_->set_process_mode(PROCESS_MODE_DISABLED);
    }
}
void MatchPresentation::retry_loading() { get_tree()->reload_current_scene(); }
void MatchPresentation::return_to_menu() { SceneNavigator::go_to_main_menu(get_tree()); }

void MatchPresentation::configure_hud(HUD *hud, const UnitDataLoader &content, godot::Node2D *base, UnitSelectionController *selection) {
    hud_ = hud;
    selection_ = selection;
    const auto [top, bottom] = protected_world_band(content, selected_units_);
    protected_top_ = top;
    protected_bottom_ = bottom;
    sky_left_ = std::clamp(static_cast<float>(protected_base_bounds(base).get_end().x) / world_size_.width, 0.0F, 1.0F);
    notify_content(ui_);
    apply_layout();
    // The HUD and pause overlay are now mounted; measure their content on the next layout pass.
    invalidate_layout();
}
void MatchPresentation::apply_layout() {
    if (host_ == nullptr) {
        return;
    }
    auto local = display_;
    local.content = {.width = get_size().x, .height = get_size().y};
    local.safe_area_applied = true;
    auto data = UiThemeProvider::data().responsive;
    data.sky_bottom = std::min(data.sky_bottom, protected_top_);
    data.bottom_top = std::max(data.bottom_top, protected_bottom_);
    const auto &card_data = is_phone_landscape(local, data) ? UiThemeProvider::phone_landscape_data().responsive : data;
    const auto card = UiThemeProvider::profile() == UiProfile::Small ? card_data.small_card : card_data.standard_card;
    const float full_field_width = std::min(get_size().x, get_size().y * world_size_.width / world_size_.height);
    const float field_left = (get_size().x - full_field_width) / 2;
    const float metrics_width =
        get_size().y > get_size().x ? get_size().x - (2 * data.margin) : get_size().x - field_left - (full_field_width * sky_left_) - (2 * data.margin);
    const auto context = resolve_ui_context(local, data, UiThemeProvider::profile());
    const bool desktop = context.desktop_match;
    if (hud_ != nullptr) {
        auto theme_context = UiThemeContext::Default;
        if (desktop) {
            theme_context = UiThemeContext::DesktopMatch;
        } else if (context.phone_landscape) {
            theme_context = UiThemeContext::PhoneLandscape;
        }
        hud_->apply_appearance(UiThemeProvider::appearance(theme_context));
    }
    const auto measured = hud_ == nullptr || desktop ? HudLayoutMetrics{} : hud_->measure_layout(metrics_width);
    const auto roster = hud_ == nullptr ? 0 : hud_->roster_size();
    const auto layout = desktop ? resolve_desktop_match_layout(local, UiThemeProvider::desktop_match_data().responsive, world_size_, roster)
                                : resolve_match_layout(local, data, card, world_size_, roster, measured, sky_left_);
    host_->set_position({layout.battlefield.x, layout.battlefield.y});
    const float density = desktop ? std::max(1.0F, display_.render_density) : 1;
    host_->set_scale({1 / density, 1 / density});
    host_->set_size({layout.battlefield.width * density, layout.battlefield.height * density});
    if (hud_ != nullptr) {
        hud_->apply_layout(layout);
    }
    if (selection_ != nullptr) {
        selection_->set_presentation_scale(layout.battlefield.width / world_size_.width, display_.touch);
    }
}
} // namespace defn
