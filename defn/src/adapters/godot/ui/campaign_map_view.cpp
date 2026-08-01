// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "campaign_map_view.h"

#include "campaign_map_data_loader.h"
#include "campaign_map_node_view.h"
#include "campaign_map_view_model.h"
#include "data_paths.h"
#include "godot_string.h"
#include "level_loader.h"
#include "operation_dossier_view.h"
#include "progression_service.h"
#include "ui_screen_scaffold.h"
#include "ui_sfx_player.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line2d.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace defn {

using namespace godot;
using GColor = godot::Color;
using GVector2 = godot::Vector2;

namespace {

constexpr float REFERENCE_WIDTH = 1920.0F;
constexpr float REFERENCE_HEIGHT = 1080.0F;

GVector2 mission_center(const CampaignMissionViewModel &mission) { return {mission.position_x * REFERENCE_WIDTH, mission.position_y * REFERENCE_HEIGHT}; }

GColor route_color(CampaignRouteState state) {
    switch (state) {
    case CampaignRouteState::COMPLETED:
        return UiThemeProvider::color("state_success");
    case CampaignRouteState::FRONTIER:
        return UiThemeProvider::color("accent");
    case CampaignRouteState::LOCKED:
        return UiThemeProvider::color("route_locked");
    }
    return UiThemeProvider::color("state_locked");
}

GColor ambience_color(CampaignMapAmbience ambience) {
    switch (ambience) {
    case CampaignMapAmbience::DUST:
        return UiThemeProvider::color("ambience_dust");
    case CampaignMapAmbience::SPORES:
        return UiThemeProvider::color("ambience_spores");
    case CampaignMapAmbience::MIST:
        return UiThemeProvider::color("ambience_mist");
    case CampaignMapAmbience::SNOW:
        return UiThemeProvider::color("ambience_snow");
    case CampaignMapAmbience::EMBERS:
        return UiThemeProvider::color("ambience_embers");
    case CampaignMapAmbience::UNKNOWN:
        return UiThemeProvider::color("ambience_dust");
    }
    return UiThemeProvider::color("ambience_dust");
}

PackedVector2Array route_points(const GVector2 &start, const GVector2 &end) {
    const GVector2 delta = end - start;
    GVector2 perpendicular(-delta.y, delta.x);
    if (perpendicular.length_squared() > 0.0F) {
        perpendicular = perpendicular.normalized() * std::clamp(delta.length() * 0.06F, 18.0F, 32.0F);
    }
    PackedVector2Array points;
    points.push_back(start);
    points.push_back(start.lerp(end, 0.5F) + perpendicular);
    points.push_back(end);
    return points;
}

real_t metric(const char *name, int fallback) { return static_cast<real_t>(UiThemeProvider::data().metric(name, fallback)); }

} // namespace

CampaignMapView::CampaignMapView() {
    UiThemeProvider::apply_to(this);
    set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    set_mouse_filter(MOUSE_FILTER_PASS);
    set_process_unhandled_input(true);
    set_process(false);
}

void CampaignMapView::_bind_methods() {}

void CampaignMapView::_ready() { focus_selected_node(); }

void CampaignMapView::_process(double delta) {
    update_loading_animation(delta);
    switch (loading_state_) {
    case LoadingState::WaitingToStart:
        begin_loading();
        break;
    case LoadingState::LoadingTextures:
        poll_texture_requests();
        break;
    case LoadingState::Ready:
    case LoadingState::Failed:
        break;
    }
}

void CampaignMapView::_notification(int what) {
    if (what == NOTIFICATION_RESIZED) {
        layout_reference_surface();
    }
}

void CampaignMapView::configure(ProgressionService *progression, const Callable &deploy_action, const Callable &back_action, UiSfxPlayer *ui_sfx_player) {
    progression_ = progression;
    supplied_view_model_.reset();
    configure_loading(deploy_action, back_action, ui_sfx_player);
}

void CampaignMapView::configure(CampaignMapViewModel view_model, const Callable &deploy_action, const Callable &back_action, UiSfxPlayer *ui_sfx_player) {
    progression_ = nullptr;
    supplied_view_model_ = std::move(view_model);
    configure_loading(deploy_action, back_action, ui_sfx_player);
}

void CampaignMapView::configure_loading(const Callable &deploy_action, const Callable &back_action, UiSfxPlayer *ui_sfx_player) {
    deploy_action_ = deploy_action;
    back_action_ = back_action;
    ui_sfx_player_ = ui_sfx_player;
    view_model_ = {};
    selected_level_id_.clear();
    requested_texture_paths_.clear();
    loaded_textures_.clear();
    loading_state_ = LoadingState::WaitingToStart;
    build_loading_overlay();
    set_process(true);
}

void CampaignMapView::build_loading_overlay() {
    const UiScreenScaffold scaffold = build_screen(this, {.show_backdrop = true, .panelled_body = false, .scrollable_body = false});
    loading_overlay_ = scaffold.root;
    loading_overlay_->set_name("LoadingOverlay");
    loading_overlay_->set_mouse_filter(MOUSE_FILTER_STOP);

    loading_spinner_ = make_label("|", "screen_display");
    loading_spinner_->set_name("LoadingSpinner");
    set_state_tint(loading_spinner_, "accent");
    loading_spinner_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    scaffold.body->add_child(loading_spinner_);

    loading_status_ = make_label("Establishing command link...", "tagline");
    loading_status_->set_name("LoadingStatus");
    loading_status_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    scaffold.body->add_child(loading_status_);

    loading_actions_ = scaffold.footer;
    loading_actions_->set_name("LoadingActions");
    loading_actions_->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    loading_actions_->hide();

    auto *retry = make_button("Retry", "secondary", callable_mp(this, &CampaignMapView::retry_loading), ui_sfx_player_);
    loading_actions_->add_child(retry);

    auto *back = make_button("Back", "secondary", callable_mp(this, &CampaignMapView::request_back), ui_sfx_player_);
    loading_actions_->add_child(back);
}

void CampaignMapView::begin_loading() {
    if (!compose_view_model() || !queue_texture_requests()) {
        return;
    }
    loading_state_ = LoadingState::LoadingTextures;
    loading_status_->set_text("Loading campaign imagery...");
}

bool CampaignMapView::compose_view_model() {
    if (supplied_view_model_.has_value()) {
        view_model_ = *supplied_view_model_;
        if (view_model_.missions.empty()) {
            fail_loading("No campaign operations are available.");
            return false;
        }
        return true;
    }
    if (progression_ == nullptr) {
        fail_loading("Campaign progression is unavailable.");
        return false;
    }

    const auto loaded_map = CampaignMapDataLoader::load(DataPaths::CAMPAIGN_MAP_DATA);
    if (!loaded_map.has_value()) {
        fail_loading("Campaign definitions could not be read.");
        return false;
    }

    std::vector<CampaignLevelPresentationSource> levels;
    const auto level_data = progression_->get_level_unlock_data();
    levels.reserve(level_data.size());
    const std::string frontier_level_id = progression_->get_frontier_level_id();
    for (const auto &level : level_data) {
        const auto definition = LevelLoader::load(DataPaths::level_definition(to_godot_string(level.level_id)));
        if (!definition.has_value()) {
            UtilityFunctions::printerr("CampaignMapView: Failed to load level definition: ", to_godot_string(level.level_id));
            fail_loading("Campaign definitions are incomplete.");
            return false;
        }
        levels.push_back(CampaignLevelPresentationSource{
            .level_id = level.level_id,
            .definition = *definition,
            .requires_completed = level.requires_completed,
            .unlocked = progression_->is_level_unlocked(level.level_id),
            .completed = progression_->is_level_completed(level.level_id),
            .frontier = frontier_level_id == level.level_id,
            .best_score = progression_->get_highest_level_score(level.level_id),
            .effective_starting_energy = progression_->get_effective_starting_energy(definition->starting_core_resource),
            .effective_base_integrity = progression_->get_effective_base_integrity(definition->base_integrity),
        });
    }

    if (levels.empty()) {
        fail_loading("No campaign operations are available.");
        return false;
    }
    view_model_ = CampaignMapPresenter::present(*loaded_map, levels);
    return true;
}

bool CampaignMapView::queue_texture_requests() {
    requested_texture_paths_.clear();
    loaded_textures_.clear();
    if (view_model_.background.path.empty() ||
        std::ranges::any_of(view_model_.missions, [](const CampaignMissionViewModel &mission) { return mission.preview.texture.path.empty(); })) {
        fail_loading("Campaign imagery is not configured.");
        return false;
    }

    std::unordered_set<std::string> unique_paths;
    unique_paths.insert(view_model_.background.path);
    for (const auto &mission : view_model_.missions) {
        unique_paths.insert(mission.preview.texture.path);
    }
    auto *loader = ResourceLoader::get_singleton();
    for (const std::string &path : unique_paths) {
        const Error error = loader->load_threaded_request(to_godot_string(path), "Texture2D", false, ResourceLoader::CACHE_MODE_REUSE);
        if (error != OK) {
            UtilityFunctions::printerr("CampaignMapView: Failed to queue texture: ", to_godot_string(path), " error=", error);
            fail_loading("Campaign imagery could not be loaded.");
            return false;
        }
        requested_texture_paths_.push_back(path);
    }
    return true;
}

void CampaignMapView::poll_texture_requests() {
    auto *loader = ResourceLoader::get_singleton();
    bool all_loaded = true;
    for (const std::string &path : requested_texture_paths_) {
        if (loaded_textures_.contains(path)) {
            continue;
        }
        switch (loader->load_threaded_get_status(to_godot_string(path))) {
        case ResourceLoader::THREAD_LOAD_IN_PROGRESS:
            all_loaded = false;
            break;
        case ResourceLoader::THREAD_LOAD_LOADED: {
            const Ref<Texture2D> texture(loader->load_threaded_get(to_godot_string(path)));
            if (!texture.is_valid()) {
                fail_loading("Campaign imagery could not be loaded.");
                return;
            }
            loaded_textures_.emplace(path, texture);
            break;
        }
        case ResourceLoader::THREAD_LOAD_FAILED:
        case ResourceLoader::THREAD_LOAD_INVALID_RESOURCE:
            UtilityFunctions::printerr("CampaignMapView: Texture loading failed: ", to_godot_string(path));
            fail_loading("Campaign imagery could not be loaded.");
            return;
        }
    }
    if (all_loaded && loaded_textures_.size() == requested_texture_paths_.size()) {
        complete_loading();
    }
}

void CampaignMapView::complete_loading() {
    build_map_content(ui_sfx_player_);
    move_child(loading_overlay_, get_child_count() - 1);
    const std::string initial_selected_level_id = view_model_.initial_selected_level_id;
    select_level(to_godot_string(initial_selected_level_id));
    layout_reference_surface();
    focus_selected_node();
    loading_state_ = LoadingState::Ready;
    set_process(false);
    Ref<Tween> tween = create_tween();
    tween->tween_property(loading_overlay_, "modulate:a", 0.0F, 0.22F);
    tween->tween_callback(callable_mp(this, &CampaignMapView::finish_overlay_fade));
}

void CampaignMapView::fail_loading(const String &message) {
    loading_state_ = LoadingState::Failed;
    loading_spinner_->set_text("!");
    loading_status_->set_text(message);
    loading_actions_->show();
    set_process(false);
}

void CampaignMapView::retry_loading() {
    view_model_ = {};
    selected_level_id_.clear();
    requested_texture_paths_.clear();
    loaded_textures_.clear();
    loading_animation_elapsed_ = 0.0;
    loading_spinner_->set_text("|");
    loading_status_->set_text("Establishing command link...");
    loading_actions_->hide();
    loading_state_ = LoadingState::WaitingToStart;
    set_process(true);
}

void CampaignMapView::finish_overlay_fade() {
    if (loading_overlay_ != nullptr) {
        loading_overlay_->hide();
        loading_overlay_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    }
}

void CampaignMapView::update_loading_animation(double delta) {
    if (loading_spinner_ == nullptr || loading_state_ == LoadingState::Failed) {
        return;
    }
    constexpr std::array<const char *, 4> frames = {"|", "/", "-", "\\"};
    loading_animation_elapsed_ += delta;
    const auto frame = static_cast<std::size_t>(loading_animation_elapsed_ / 0.12) % frames.size();
    loading_spinner_->set_text(frames[frame]);
}

Ref<Texture2D> CampaignMapView::texture_for(const CampaignTextureDefinition &definition) const {
    const auto found = loaded_textures_.find(definition.path);
    return found == loaded_textures_.end() ? Ref<Texture2D>() : found->second;
}

void CampaignMapView::build_map_content(UiSfxPlayer *ui_sfx_player) {
    auto *backdrop = memnew(ColorRect);
    backdrop->set_color(UiThemeProvider::color("backdrop"));
    backdrop->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    backdrop->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(backdrop);

    reference_surface_ = memnew(Control);
    reference_surface_->set_name("ReferenceSurface");
    reference_surface_->set_size({REFERENCE_WIDTH, REFERENCE_HEIGHT});
    reference_surface_->set_clip_contents(true);
    reference_surface_->set_mouse_filter(MOUSE_FILTER_PASS);
    add_child(reference_surface_);

    Ref<Texture2D> panorama = texture_for(view_model_.background);
    auto *panorama_view = memnew(Sprite2D);
    panorama_view->set_name("Panorama");
    panorama_view->set_texture(panorama);
    panorama_view->set_centered(false);
    panorama_view->set_position({0.0F, 0.0F});
    panorama_view->set_texture_filter(CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
    if (panorama.is_valid() && panorama->get_width() > 0 && panorama->get_height() > 0) {
        panorama_view->set_scale({REFERENCE_WIDTH / static_cast<float>(panorama->get_width()), REFERENCE_HEIGHT / static_cast<float>(panorama->get_height())});
    }
    reference_surface_->add_child(panorama_view);

    auto *vignette = memnew(ColorRect);
    vignette->set_color(UiThemeProvider::color("scrim_soft"));
    vignette->set_position({0.0F, 0.0F});
    vignette->set_size({REFERENCE_WIDTH, REFERENCE_HEIGHT});
    vignette->set_mouse_filter(MOUSE_FILTER_IGNORE);
    reference_surface_->add_child(vignette);

    auto *map_layer = memnew(Control);
    map_layer->set_name("MapInteractionLayer");
    map_layer->set_position({0.0F, 0.0F});
    map_layer->set_size({REFERENCE_WIDTH, REFERENCE_HEIGHT});
    map_layer->set_mouse_filter(MOUSE_FILTER_PASS);
    reference_surface_->add_child(map_layer);

    auto *route_layer = memnew(Control);
    route_layer->set_name("RouteLayer");
    route_layer->set_position({0.0F, 0.0F});
    route_layer->set_size({REFERENCE_WIDTH, REFERENCE_HEIGHT});
    route_layer->set_mouse_filter(MOUSE_FILTER_IGNORE);
    map_layer->add_child(route_layer);
    build_routes(route_layer);

    ambience_ = memnew(CPUParticles2D);
    ambience_->set_name("Ambience");
    ambience_->set_amount(24);
    ambience_->set_lifetime(4.0);
    ambience_->set_pre_process_time(4.0);
    ambience_->set_randomness_ratio(0.85F);
    ambience_->set_emission_shape(CPUParticles2D::EMISSION_SHAPE_RECTANGLE);
    ambience_->set_emission_rect_extents({220.0F, 130.0F});
    ambience_->set_direction({0.2F, -1.0F});
    ambience_->set_spread(55.0F);
    ambience_->set_param_min(CPUParticles2D::PARAM_INITIAL_LINEAR_VELOCITY, 6.0F);
    ambience_->set_param_max(CPUParticles2D::PARAM_INITIAL_LINEAR_VELOCITY, 18.0F);
    ambience_->set_param_min(CPUParticles2D::PARAM_SCALE, 1.0F);
    ambience_->set_param_max(CPUParticles2D::PARAM_SCALE, 2.4F);
    ambience_->set_gravity({0.0F, -2.0F});
    map_layer->add_child(ambience_);

    auto *node_layer = memnew(Control);
    node_layer->set_name("MissionNodes");
    node_layer->set_position({0.0F, 0.0F});
    node_layer->set_size({REFERENCE_WIDTH, REFERENCE_HEIGHT});
    node_layer->set_mouse_filter(MOUSE_FILTER_PASS);
    map_layer->add_child(node_layer);
    build_nodes(node_layer, ui_sfx_player);

    auto *header_backplate = memnew(ColorRect);
    header_backplate->set_name("HeaderBackplate");
    header_backplate->set_color(UiThemeProvider::color("scrim_panel"));
    header_backplate->set_position({0.0F, 0.0F});
    header_backplate->set_size({REFERENCE_WIDTH, metric("map_header_height", 104)});
    header_backplate->set_mouse_filter(MOUSE_FILTER_IGNORE);
    reference_surface_->add_child(header_backplate);

    auto *hints_backplate = memnew(ColorRect);
    hints_backplate->set_name("HintsBackplate");
    hints_backplate->set_color(UiThemeProvider::color("scrim_panel"));
    hints_backplate->set_position({0.0F, 990.0F});
    hints_backplate->set_size({1392.0F, 90.0F});
    hints_backplate->set_mouse_filter(MOUSE_FILTER_IGNORE);
    reference_surface_->add_child(hints_backplate);

    auto *breadcrumb = make_label("CAMPAIGN / THE EASTERN EXPEDITION", "screen_heading");
    breadcrumb->set_name("Breadcrumb");
    breadcrumb->set_position({64.0F, 34.0F});
    breadcrumb->set_size({900.0F, 48.0F});
    reference_surface_->add_child(breadcrumb);

    auto *secured = make_label(vformat("%d / %d SECURED", view_model_.completed_count, view_model_.missions.size()), "screen_heading");
    secured->set_name("SecuredCount");
    set_state_tint(secured, "state_success");
    secured->set_position({1150.0F, 40.0F});
    secured->set_size({220.0F, 40.0F});
    secured->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    reference_surface_->add_child(secured);

    auto *close = make_button(String::utf8("×"), "close", callable_mp(this, &CampaignMapView::request_back), ui_sfx_player);
    close->set_name("CloseButton");
    close->set_flat(true);
    close->set_position({1812.0F, 30.0F});
    close->set_size({48.0F, 48.0F});
    close->set_focus_mode(FOCUS_ALL);
    reference_surface_->add_child(close);

    dossier_ = memnew(OperationDossierView);
    dossier_->set_name("OperationDossier");
    dossier_->set_position({metric("map_dossier_x", 1408), metric("map_dossier_y", 124)});
    dossier_->set_size({metric("operation_dossier_width", 464), metric("operation_dossier_height", 866)});
    dossier_->connect("deploy_requested", callable_mp(this, &CampaignMapView::activate_level));
    dossier_->connect("back_requested", callable_mp(this, &CampaignMapView::request_back));
    dossier_->attach_sfx(ui_sfx_player);
    reference_surface_->add_child(dossier_);

    auto *hints = make_label(String::utf8("[Esc] Back     [\u2190/\u2192] Choose operation     [Enter] Inspect / Deploy"), "tagline");
    hints->set_name("InputHints");
    hints->set_position({72.0F, 1010.0F});
    hints->set_size({1050.0F, 36.0F});
    reference_surface_->add_child(hints);
}

void CampaignMapView::build_routes(Control *route_layer) {
    for (const CampaignRouteViewModel &route : view_model_.routes) {
        if (route.from_index >= view_model_.missions.size() || route.to_index >= view_model_.missions.size()) {
            continue;
        }
        const PackedVector2Array points =
            route_points(mission_center(view_model_.missions[route.from_index]), mission_center(view_model_.missions[route.to_index]));
        auto *shadow = memnew(Line2D);
        shadow->set_name(vformat("RouteShadow%d", route.from_index));
        shadow->set_points(points);
        shadow->set_width(8.0F);
        shadow->set_default_color(UiThemeProvider::color("route_shadow"));
        shadow->set_antialiased(true);
        route_layer->add_child(shadow);

        auto *line = memnew(Line2D);
        line->set_name(vformat("RouteSegment%d", route.from_index));
        line->set_points(points);
        line->set_width(route.state == CampaignRouteState::LOCKED ? 3.0F : 4.0F);
        line->set_default_color(route_color(route.state));
        line->set_antialiased(true);
        route_layer->add_child(line);
    }
}

void CampaignMapView::build_nodes(Control *node_layer, UiSfxPlayer *ui_sfx_player) {
    node_views_.reserve(view_model_.missions.size());
    for (const CampaignMissionViewModel &mission : view_model_.missions) {
        auto *node = memnew(CampaignMapNodeView);
        node->set_name(to_godot_string(mission.level_id));
        node->set_position(mission_center(mission) - GVector2(94.0F, 67.0F));
        node->configure(mission, texture_for(mission.preview.texture));
        node->connect("selected", callable_mp(this, &CampaignMapView::select_level));
        node->connect("activated", callable_mp(this, &CampaignMapView::activate_level));
        if (ui_sfx_player != nullptr) {
            ui_sfx_player->connect_menu_button(node->button());
        }
        node_layer->add_child(node);
        node_views_.push_back(node);
    }
}

void CampaignMapView::select_level(const String &level_id) {
    const std::string selected = to_std_string(level_id);
    const CampaignMissionViewModel *mission = find_mission(selected);
    if (mission == nullptr || dossier_ == nullptr) {
        return;
    }
    if (selected == selected_level_id_) {
        return;
    }
    selected_level_id_ = selected;
    for (CampaignMapNodeView *node : node_views_) {
        node->set_selected(node->level_id() == selected_level_id_);
    }
    dossier_->configure(*mission, texture_for(mission->preview.texture));
    configure_ambience(*mission);
    dossier_->set_modulate(GColor(1, 1, 1, 0.82F));
    Ref<Tween> tween = create_tween();
    tween->tween_property(dossier_, "modulate:a", 1.0F, 0.16F);
}

void CampaignMapView::activate_level(const String &level_id) {
    select_level(level_id);
    deploy_selected();
}

void CampaignMapView::deploy_selected() {
    const CampaignMissionViewModel *mission = find_mission(selected_level_id_);
    if (mission == nullptr || mission->state == CampaignNodeState::LOCKED || !deploy_action_.is_valid()) {
        return;
    }
    deploy_action_.call(to_godot_string(selected_level_id_));
}

void CampaignMapView::request_back() {
    if (back_action_.is_valid()) {
        back_action_.call();
    }
}

void CampaignMapView::select_relative(int offset) {
    if (view_model_.missions.empty()) {
        return;
    }
    const auto found =
        std::ranges::find_if(view_model_.missions, [this](const CampaignMissionViewModel &mission) { return mission.level_id == selected_level_id_; });
    const std::ptrdiff_t current = found == view_model_.missions.end() ? 0 : std::distance(view_model_.missions.begin(), found);
    const auto count = static_cast<std::ptrdiff_t>(view_model_.missions.size());
    const std::ptrdiff_t next = (current + offset + count) % count;
    select_level(to_godot_string(view_model_.missions[static_cast<std::size_t>(next)].level_id));
    node_views_[static_cast<std::size_t>(next)]->grab_node_focus();
}

void CampaignMapView::_unhandled_input(const Ref<InputEvent> &event) {
    if (loading_state_ != LoadingState::Ready || !event.is_valid() || event->is_echo()) {
        return;
    }
    if (event->is_action_pressed("ui_cancel")) {
        request_back();
        get_viewport()->set_input_as_handled();
    } else if (event->is_action_pressed("ui_left") || event->is_action_pressed("ui_up")) {
        select_relative(-1);
        get_viewport()->set_input_as_handled();
    } else if (event->is_action_pressed("ui_right") || event->is_action_pressed("ui_down")) {
        select_relative(1);
        get_viewport()->set_input_as_handled();
    } else if (event->is_action_pressed("ui_accept")) {
        deploy_selected();
        get_viewport()->set_input_as_handled();
    }
}

void CampaignMapView::layout_reference_surface() {
    if (reference_surface_ == nullptr || get_size().x <= 0.0F || get_size().y <= 0.0F) {
        return;
    }
    const float scale = std::min(get_size().x / REFERENCE_WIDTH, get_size().y / REFERENCE_HEIGHT);
    reference_surface_->set_scale({scale, scale});
    reference_surface_->set_position((get_size() - GVector2(REFERENCE_WIDTH * scale, REFERENCE_HEIGHT * scale)) * 0.5F);
}

void CampaignMapView::focus_selected_node() {
    if (!is_inside_tree()) {
        return;
    }
    for (CampaignMapNodeView *node : node_views_) {
        if (node != nullptr && node->level_id() == selected_level_id_) {
            node->grab_node_focus();
            return;
        }
    }
}

void CampaignMapView::configure_ambience(const CampaignMissionViewModel &mission) {
    if (ambience_ == nullptr) {
        return;
    }
    ambience_->set_position(mission_center(mission));
    ambience_->set_color(ambience_color(mission.ambience));
    ambience_->restart();
}

const CampaignMissionViewModel *CampaignMapView::find_mission(const std::string &level_id) const {
    const auto found =
        std::ranges::find_if(view_model_.missions, [&level_id](const CampaignMissionViewModel &mission) { return mission.level_id == level_id; });
    return found == view_model_.missions.end() ? nullptr : &*found;
}

} // namespace defn
