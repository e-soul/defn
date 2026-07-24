#include "campaign_map_view.h"

#include "campaign_map_node_view.h"
#include "campaign_texture_cache.h"
#include "godot_string.h"
#include "operation_dossier_view.h"
#include "ui_sfx_player.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line2d.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

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
        return {"5fcb9a"};
    case CampaignRouteState::FRONTIER:
        return {"f2be55"};
    case CampaignRouteState::LOCKED:
        return {0.35F, 0.39F, 0.42F, 0.48F};
    }
    return {"59646c"};
}

GColor ambience_color(CampaignMapAmbience ambience) {
    switch (ambience) {
    case CampaignMapAmbience::DUST:
        return {0.82F, 0.65F, 0.36F, 0.42F};
    case CampaignMapAmbience::SPORES:
        return {0.48F, 0.78F, 0.5F, 0.4F};
    case CampaignMapAmbience::MIST:
        return {0.55F, 0.82F, 0.86F, 0.3F};
    case CampaignMapAmbience::SNOW:
        return {0.82F, 0.91F, 1.0F, 0.52F};
    case CampaignMapAmbience::EMBERS:
        return {0.95F, 0.4F, 0.22F, 0.48F};
    case CampaignMapAmbience::UNKNOWN:
        return {0.82F, 0.65F, 0.36F, 0.42F};
    }
    return {0.82F, 0.65F, 0.36F, 0.42F};
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

Label *make_header_label(const String &text, int size, const GColor &color) {
    auto *label = memnew(Label);
    label->set_text(text);
    label->add_theme_font_size_override("font_size", size);
    label->add_theme_color_override("font_color", color);
    label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    return label;
}

} // namespace

CampaignMapView::CampaignMapView() {
    set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    set_mouse_filter(MOUSE_FILTER_PASS);
    set_process_unhandled_input(true);
    texture_cache_.instantiate();
}

void CampaignMapView::_bind_methods() {}

void CampaignMapView::_ready() { focus_selected_node(); }

void CampaignMapView::_notification(int what) {
    if (what == NOTIFICATION_RESIZED) {
        layout_reference_surface();
    }
}

void CampaignMapView::configure(CampaignMapViewModel view_model, const Callable &deploy_action, const Callable &back_action, UiSfxPlayer *ui_sfx_player) {
    view_model_ = std::move(view_model);
    deploy_action_ = deploy_action;
    back_action_ = back_action;
    selected_level_id_ = view_model_.initial_selected_level_id;
    build_screen(ui_sfx_player);
    select_level(to_godot_string(selected_level_id_));
    layout_reference_surface();
    focus_selected_node();
}

void CampaignMapView::build_screen(UiSfxPlayer *ui_sfx_player) {
    auto *backdrop = memnew(ColorRect);
    backdrop->set_color(GColor("0a1118"));
    backdrop->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    backdrop->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(backdrop);

    reference_surface_ = memnew(Control);
    reference_surface_->set_name("ReferenceSurface");
    reference_surface_->set_size({REFERENCE_WIDTH, REFERENCE_HEIGHT});
    reference_surface_->set_clip_contents(true);
    reference_surface_->set_mouse_filter(MOUSE_FILTER_PASS);
    add_child(reference_surface_);

    Ref<Texture2D> panorama = texture_cache_->load(view_model_.background);
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
    vignette->set_color(GColor(0.01F, 0.025F, 0.035F, 0.18F));
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
    header_backplate->set_color(GColor(0.02F, 0.035F, 0.05F, 0.5F));
    header_backplate->set_position({0.0F, 0.0F});
    header_backplate->set_size({REFERENCE_WIDTH, 104.0F});
    header_backplate->set_mouse_filter(MOUSE_FILTER_IGNORE);
    reference_surface_->add_child(header_backplate);

    auto *hints_backplate = memnew(ColorRect);
    hints_backplate->set_name("HintsBackplate");
    hints_backplate->set_color(GColor(0.02F, 0.035F, 0.05F, 0.58F));
    hints_backplate->set_position({0.0F, 990.0F});
    hints_backplate->set_size({1392.0F, 90.0F});
    hints_backplate->set_mouse_filter(MOUSE_FILTER_IGNORE);
    reference_surface_->add_child(hints_backplate);

    auto *breadcrumb = make_header_label("CAMPAIGN / THE EASTERN EXPEDITION", 24, GColor("e8ddc3"));
    breadcrumb->set_name("Breadcrumb");
    breadcrumb->set_position({64.0F, 34.0F});
    breadcrumb->set_size({900.0F, 48.0F});
    reference_surface_->add_child(breadcrumb);

    auto *secured = make_header_label(vformat("%d / %d SECURED", view_model_.completed_count, view_model_.missions.size()), 22, GColor("5fcb9a"));
    secured->set_name("SecuredCount");
    secured->set_position({1150.0F, 40.0F});
    secured->set_size({220.0F, 40.0F});
    secured->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    reference_surface_->add_child(secured);

    auto *close = memnew(Button);
    close->set_name("CloseButton");
    close->set_text(String::utf8("×"));
    close->set_flat(true);
    close->set_position({1812.0F, 30.0F});
    close->set_size({48.0F, 48.0F});
    close->set_focus_mode(FOCUS_ALL);
    close->add_theme_font_size_override("font_size", 32);
    close->connect("pressed", callable_mp(this, &CampaignMapView::request_back));
    if (ui_sfx_player != nullptr) {
        ui_sfx_player->connect_menu_button(close);
    }
    reference_surface_->add_child(close);

    dossier_ = memnew(OperationDossierView);
    dossier_->set_name("OperationDossier");
    dossier_->set_position({1408.0F, 124.0F});
    dossier_->set_size({464.0F, 866.0F});
    dossier_->connect("deploy_requested", callable_mp(this, &CampaignMapView::activate_level));
    dossier_->connect("back_requested", callable_mp(this, &CampaignMapView::request_back));
    if (ui_sfx_player != nullptr) {
        ui_sfx_player->connect_menu_button(dossier_->deploy_button());
        ui_sfx_player->connect_menu_button(dossier_->back_button());
    }
    reference_surface_->add_child(dossier_);

    auto *hints = make_header_label(String::utf8("[Esc] Back     [←/→] Choose operation     [Enter] Inspect / Deploy"), 18, GColor("b9c4c3"));
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
        shadow->set_default_color(GColor(0.02F, 0.04F, 0.05F, 0.9F));
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
        node->configure(mission, texture_cache_->load(mission.preview.texture));
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
    selected_level_id_ = selected;
    for (CampaignMapNodeView *node : node_views_) {
        node->set_selected(node->level_id() == selected_level_id_);
    }
    dossier_->configure(*mission, texture_cache_->load(mission->preview.texture));
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
    if (!event.is_valid() || event->is_echo()) {
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
