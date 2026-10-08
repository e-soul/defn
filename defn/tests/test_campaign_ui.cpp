// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "campaign_map_node_view.h"
#include "mobile_campaign_view.h"
#include "operation_dossier_view.h"
#include "ui_sfx_player.h"
#include "ui_test_helpers.h"
#include "ui_theme_provider.h"
#include <algorithm>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/memory.hpp>
#include <string>
#include <utility>
#include <vector>

namespace defn {
using namespace ui_test;
namespace {
void check_state_medallion(CampaignMapView *campaign_map) {
    const String medallion_path = "WideUI/MissionNodes/level_01/StateMedallion";
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
namespace {
void configure_carousel(MobileCampaignView *view, const CampaignMapViewModel &model, std::vector<Ref<Texture2D>> previews, MobileCampaignActions actions) {
    const auto observe = actions.select;
    actions.select = [view, observe](const CampaignSelection &selection) {
        view->select_item(selection);
        if (observe) {
            observe(selection);
        }
    };
    view->configure(model, std::move(previews), actions);
}
} // namespace
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
    campaign_map->configure(static_cast<ProgressionService *>(nullptr), {}, {});

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
    campaign_map->configure(std::move(view_model), {}, {});

    DEFN_CHECK(pump_campaign_map_loading(campaign_map));
    DEFN_CHECK_EQ(campaign_map->loading_state(), CampaignMapView::LoadingState::Ready);
    DEFN_CHECK_EQ(campaign_map->selected_level_id(), std::string("level_02"));
    DEFN_REQUIRE(campaign_map->dossier() != nullptr);
}

namespace {
CampaignMapViewModel mobile_campaign_fixture(bool endless = true) {
    CampaignMapViewModel model{.title = "CAMPAIGN", .initial_selected_level_id = "level_02", .completed_count = 1};
    const std::array names{"Desert Outpost", "Jungle Ruins", "Summar Beach", "The Winter Forest", "Feldkirchen"};
    for (size_t i = 0; i < names.size(); ++i) {
        model.missions.push_back({.level_id = "level_0" + std::to_string(i + 1),
                                  .sequence_number = static_cast<int>(i + 1),
                                  .name = names[i],
                                  .tagline = "Make the final stand at a fortified settlement under sustained pressure.",
                                  .state = CampaignNodeState::LOCKED,
                                  .unlock_requirement = "Secure Level 04 to open this operation.",
                                  .threat_label = "Extreme",
                                  .duration_label = "3+ MIN",
                                  .wave_count = 5});
    }
    model.missions[0].state = CampaignNodeState::COMPLETED;
    model.missions[1].state = CampaignNodeState::FRONTIER;
    if (endless) {
        model.endless = CampaignEndlessViewModel{.title = "Standing Engagement", .tagline = "Hold the line for as long as possible."};
    }
    return model;
}

void check_carousel_control(Control *control, const godot::Vector2 &available) {
    if (control == nullptr || !control->is_visible()) {
        return;
    }
    DEFN_CHECK(control->get_position().x >= 0);
    DEFN_CHECK(control->get_position().y >= 0);
    check_mobile_control_bounds(control, available);
    if (auto *button = Object::cast_to<Button>(control); button != nullptr) {
        DEFN_CHECK(button->get_size().x >= 48);
        DEFN_CHECK(button->get_size().y >= 48);
    }
}

void check_carousel_bounds(MobileCampaignView *view) {
    for (int i = 0; i < view->get_child_count(); ++i) {
        check_carousel_control(Object::cast_to<Control>(view->get_child(i)), view->get_size());
    }
    auto *card = Object::cast_to<Control>(view->get_node_or_null("MissionCard"));
    DEFN_REQUIRE(card != nullptr);
    for (int i = 0; i < card->get_child_count(); ++i) {
        check_carousel_control(Object::cast_to<Control>(card->get_child(i)), card->get_size());
    }
    auto *preview = Object::cast_to<Control>(card->get_node_or_null("MissionPreview"));
    for (const char *name : {"OperationBadge", "StatusBadge", "UnlockPlate"}) {
        auto *plate = Object::cast_to<Control>(preview->get_node_or_null(name));
        check_carousel_control(plate, preview->get_size());
        if (plate->is_visible()) {
            check_mobile_control_bounds(Object::cast_to<Control>(plate->get_child(0)), plate->get_size());
        }
    }
}

void check_button_background(Button *button, const char *state, std::string_view color) {
    const Ref<StyleBoxFlat> style = button->get_theme_stylebox(state);
    DEFN_REQUIRE(style.is_valid());
    DEFN_CHECK(style->get_bg_color() == UiThemeProvider::color(color));
}

void check_campaign_action_colors(Button *deploy, Button *back) {
    DEFN_CHECK(!deploy->is_disabled());
    for (const auto &[state, color] : {std::pair{"normal", "accent"}, {"hover", "accent_strong"}, {"pressed", "accent"}, {"focus", "accent"}}) {
        check_button_background(deploy, state, color);
    }
    DEFN_CHECK(deploy->get_theme_color("font_color") == UiThemeProvider::color("text_inverse"));
    check_button_background(back, "normal", "surface_raised");
    DEFN_CHECK(back->get_theme_color("font_color") == UiThemeProvider::color("text_primary"));
}
} // namespace

DEFN_TEST(mobile_campaign_every_card_fits_without_scrolling_and_keeps_selection_on_rotation) {
    const TreeMountedNode<MobileCampaignView> owner;
    auto *view = owner.get();
    configure_carousel(view, mobile_campaign_fixture(), {}, {});
    DEFN_CHECK_EQ(view->selected_index(), size_t(1));
    auto *next = Object::cast_to<Button>(view->get_node_or_null("NextMission"));
    auto *previous = Object::cast_to<Button>(view->get_node_or_null("PreviousMission"));
    previous->emit_signal("pressed");
    for (size_t index = 0; index < 6; ++index) {
        for (const godot::Vector2 size :
             {godot::Vector2(390, 783), godot::Vector2(412, 703), godot::Vector2(667, 322), godot::Vector2(864, 230), godot::Vector2(667, 230),
              godot::Vector2(864, 338), godot::Vector2(320, 660), godot::Vector2(320, 480), godot::Vector2(568, 230)}) {
            view->set_size(size);
            DEFN_CHECK_EQ(view->selected_index(), index);
            check_carousel_bounds(view);
            DEFN_CHECK(view->find_children("*", "ScrollContainer", true, false).is_empty());
        }
        next->emit_signal("pressed");
    }
    DEFN_CHECK_EQ(view->selected_index(), size_t(5));
    DEFN_CHECK(has_all_labels(view, {"STANDING ENGAGEMENT", "UNBOUNDED", "UNTIL THE BASE FALLS"}));
    DEFN_CHECK_EQ(find_button_by_text(view, "BEGIN WATCH")->is_disabled(), false);
}

DEFN_TEST(mobile_campaign_actions_keep_their_colors_when_configured_before_mounting) {
    const TreeMountedNode<Control> host;
    auto *view = memnew(MobileCampaignView);
    configure_carousel(view, mobile_campaign_fixture(false), {}, {});
    host.get()->add_child(view);
    auto *deploy = Object::cast_to<Button>(view->get_node_or_null("CampaignDeploy"));
    auto *back = Object::cast_to<Button>(view->get_node_or_null("CampaignBack"));
    DEFN_REQUIRE(deploy != nullptr);
    DEFN_REQUIRE(back != nullptr);
    for (const godot::Vector2 size : {godot::Vector2(390, 783), godot::Vector2(667, 322), godot::Vector2(390, 783)}) {
        view->set_size(size);
        check_campaign_action_colors(deploy, back);
    }
    view->select_level("level_03");
    DEFN_CHECK(deploy->is_disabled());
    check_button_background(deploy, "disabled", "surface_sunken");
    DEFN_CHECK(deploy->get_theme_color("font_disabled_color") == UiThemeProvider::color("text_disabled"));
}

DEFN_TEST(mobile_campaign_browsing_never_deploys_and_locked_cards_cannot_deploy) {
    const TreeMountedNode<MobileCampaignView> owner;
    auto *view = owner.get();
    GodotObjectOwner<Label> selected_marker(memnew(Label));
    GodotObjectOwner<Label> deployed_marker(memnew(Label));
    configure_carousel(
        view, mobile_campaign_fixture(false), {},
        {.select = [marker = selected_marker.get()](const CampaignSelection &selection) { marker->set_text(String(selection.level_id.c_str())); },
         .deploy = Callable(deployed_marker.get(), "set_text").bind("mission"),
         .endless = Callable(deployed_marker.get(), "set_text").bind("endless")});
    view->set_size({390, 783});
    find_button_by_text(view, ">")->emit_signal("pressed");
    DEFN_CHECK_EQ(selected_marker->get_text(), String("level_03"));
    DEFN_CHECK_EQ(deployed_marker->get_text(), String());
    DEFN_CHECK(has_label_text(view, "Secure Level 04 to open this operation."));
    auto *deploy = Object::cast_to<Button>(view->get_node_or_null("CampaignDeploy"));
    DEFN_CHECK(deploy->is_disabled());
    deploy->emit_signal("pressed"); // The action also checks the state, even if invoked outside GUI dispatch.
    DEFN_CHECK_EQ(deployed_marker->get_text(), String());
    find_button_by_text(view, "<")->emit_signal("pressed");
    deploy->emit_signal("pressed");
    DEFN_CHECK_EQ(deployed_marker->get_text(), String("mission"));
    for (int i = 0; i < 6; ++i) {
        find_button_by_text(view, ">")->emit_signal("pressed");
    }
    DEFN_CHECK_EQ(view->selected_index(), size_t(4));
    DEFN_CHECK(find_button_by_text(view, "BEGIN WATCH") == nullptr);
}

DEFN_TEST(mobile_campaign_swipes_select_once_and_resize_cancels_the_gesture) {
    const TreeMountedNode<MobileCampaignView> owner;
    auto *view = owner.get();
    configure_carousel(view, mobile_campaign_fixture(), {}, {.endless = Callable(view, "set_meta").bind("endless", true)});
    view->set_size({390, 783});
    Ref<InputEventMouseButton> press;
    press.instantiate();
    press->set_button_index(MOUSE_BUTTON_LEFT);
    press->set_pressed(true);
    press->set_position({280, 220});
    Ref<InputEventMouseButton> release;
    release.instantiate();
    release->set_button_index(MOUSE_BUTTON_LEFT);
    release->set_position(view->get_global_transform_with_canvas().xform({100, 220}));
    view->_gui_input(press);
    view->_input(release);
    DEFN_CHECK_EQ(view->selected_index(), size_t(2));
    view->_input(release);
    DEFN_CHECK_EQ(view->selected_index(), size_t(2));
    view->_gui_input(press);
    view->set_size({864, 230});
    view->_input(release);
    DEFN_CHECK_EQ(view->selected_index(), size_t(2));
    view->select_level("level_05");
    find_button_by_text(view, ">")->emit_signal("pressed");
    DEFN_CHECK(!view->has_meta("endless"));
    find_button_by_text(view, "BEGIN WATCH")->emit_signal("pressed");
    DEFN_CHECK(view->has_meta("endless"));
}

namespace {

/// A one-mission map carrying the endless entry, ready for inspection.
CampaignMapView *build_beacon_map(GodotObjectOwner<CampaignMapView> &owner) {
    CampaignMapView *campaign_map = owner.get();
    const CampaignTextureDefinition texture{.path = "res://assets/campaign/desert_outpost_preview.jpg"};
    CampaignMapViewModel view_model{
        .background = texture,
        .missions = {{.level_id = "level_01", .name = "First", .preview = {.texture = texture}}},
        .endless = CampaignEndlessViewModel{.title = "Standing Engagement",
                                            .tagline = "Hold the line.",
                                            .preview = {.texture = texture},
                                            .position_x = 0.685F,
                                            .position_y = 0.5F,
                                            .best_wave = 17,
                                            .best_score = 4820,
                                            .base_starting_energy = 105,
                                            .effective_starting_energy = 125,
                                            .base_integrity = 4,
                                            .effective_base_integrity = 5,
                                            .record_label = "BEST  WAVE 17  /  4820",
                                            .route_from_index = 0},
        .initial_selected_level_id = "level_01",
    };
    campaign_map->configure(std::move(view_model), {}, {});
    return pump_campaign_map_loading(campaign_map) ? campaign_map : nullptr;
}

} // namespace

DEFN_TEST(campaign_map_shows_an_endless_mode_button_centred_on_the_header) {
    GodotObjectOwner<CampaignMapView> owner(memnew(CampaignMapView));
    CampaignMapView *campaign_map = build_beacon_map(owner);
    DEFN_REQUIRE(campaign_map != nullptr);

    Node *header_row = campaign_map->get_node_or_null("WideUI/HeaderRow");
    DEFN_REQUIRE(header_row != nullptr);
    Button *endless_button = find_card_by_title(header_row, "Endless Mode");
    DEFN_REQUIRE(endless_button != nullptr);
    DEFN_CHECK_EQ(endless_button->get_name(), String("EndlessButton"));

    // The breadcrumb and the secured count sit either side of the button in the header row, so it lands between
    // them rather than hugging one edge -- an HBoxContainer places children in child order, so this pins that
    // order without depending on a layout pass having already run.
    auto *breadcrumb = Object::cast_to<Control>(header_row->get_node_or_null("Breadcrumb"));
    auto *secured = Object::cast_to<Control>(header_row->get_node_or_null("SecuredCount"));
    DEFN_REQUIRE(breadcrumb != nullptr);
    DEFN_REQUIRE(secured != nullptr);
    DEFN_CHECK(breadcrumb->get_index() < endless_button->get_index());
    DEFN_CHECK(endless_button->get_index() < secured->get_index());
}

DEFN_TEST(campaign_map_desktop_selection_updates_the_carousel_even_after_browsing_endless) {
    GodotObjectOwner<CampaignMapView> owner(memnew(CampaignMapView));
    auto *map = build_beacon_map(owner);
    DEFN_REQUIRE(map != nullptr);
    map->set_size({390, 783});
    auto *mobile = Object::cast_to<MobileCampaignView>(map->get_node_or_null("MobileCampaign"));
    DEFN_REQUIRE(mobile != nullptr);
    find_button_by_text(mobile, ">")->emit_signal("pressed");
    DEFN_CHECK_EQ(mobile->selected_index(), size_t(1));
    // The wide map still remembers the previous regular mission. Explicitly selecting it
    // must return the compact carousel to that mission, even though its ID has not changed.
    auto *interaction = Object::cast_to<Button>(map->get_node_or_null("WideUI/MissionNodes/level_01/Interaction"));
    DEFN_REQUIRE(interaction != nullptr);
    interaction->emit_signal("pressed");
    DEFN_CHECK_EQ(mobile->selected_index(), size_t(0));
    DEFN_CHECK_EQ(map->selected_level_id(), std::string("level_01"));
}

DEFN_TEST(campaign_map_endless_button_deploys_directly_without_opening_the_dossier) {
    GodotObjectOwner<CampaignMapView> owner(memnew(CampaignMapView));
    GodotObjectOwner<Button> deployed_marker(memnew(Button));
    CampaignMapView *campaign_map = build_beacon_map(owner);
    DEFN_REQUIRE(campaign_map != nullptr);
    deployed_marker.get()->show();
    campaign_map->set_endless_action(Callable(deployed_marker.get(), "hide"));

    Node *header_row = campaign_map->get_node_or_null("WideUI/HeaderRow");
    DEFN_REQUIRE(header_row != nullptr);
    Button *endless_button = find_card_by_title(header_row, "Endless Mode");
    DEFN_REQUIRE(endless_button != nullptr);
    OperationDossierView *dossier = campaign_map->dossier();
    DEFN_REQUIRE(dossier != nullptr);

    // Pressing the header button deploys straight into the run: there is no endless variant to compare it
    // against the way a mission choice has siblings, so the dossier stays exactly as it was.
    endless_button->emit_signal("pressed");

    DEFN_CHECK(!deployed_marker.get()->is_visible());
    DEFN_CHECK(!has_label_containing(dossier, "STANDING ENGAGEMENT"));
}

DEFN_TEST(campaign_map_panorama_fills_and_clips_reference_surface) {
    const TreeMountedNode<MenuManager> menu_manager_owner;
    CampaignMapView *campaign_map = show_campaign_map(menu_manager_owner);

    DEFN_REQUIRE(campaign_map != nullptr);
    DEFN_CHECK(Object::cast_to<Control>(campaign_map->get_parent()) != nullptr);
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
    auto *desert_node = Object::cast_to<Control>(campaign_map->get_node_or_null("WideUI/MissionNodes/level_01"));
    DEFN_REQUIRE(desert_node != nullptr);
    DEFN_CHECK_EQ(desert_node->get_size(), godot::Vector2(188.0F, 134.0F));
    DEFN_CHECK(campaign_map->get_node_or_null("WideUI/MissionNodes/level_01/LabelPlate") == nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("WideUI/MissionNodes/level_01/MissionName") == nullptr);
    DEFN_CHECK(campaign_map->get_node_or_null("WideUI/MissionNodes/level_01/MissionDetail") == nullptr);
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
    campaign_map->configure(std::move(view_model), Callable(deployment_recorder.get(), "set_text"), {});

    DEFN_REQUIRE(pump_campaign_map_loading(campaign_map));
    auto *first = Object::cast_to<Button>(campaign_map->get_node_or_null("WideUI/MissionNodes/level_01/Interaction"));
    auto *second = Object::cast_to<Button>(campaign_map->get_node_or_null("WideUI/MissionNodes/level_02/Interaction"));
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
    DEFN_CHECK(campaign_map->get_node_or_null("WideUI/MissionNodes/level_04/PostcardFrame") != nullptr);
    check_state_medallion(campaign_map);
    auto *node_interaction = Object::cast_to<Button>(campaign_map->get_node_or_null("WideUI/MissionNodes/level_01/Interaction"));
    DEFN_REQUIRE(node_interaction != nullptr);
    DEFN_CHECK_EQ(node_interaction->get_focus_mode(), Control::FOCUS_NONE);
    DEFN_CHECK(!node_interaction->has_theme_stylebox_override("focus"));
    DEFN_CHECK(has_label_text(campaign_map, "Grime"));
    DEFN_CHECK(!has_label_text(campaign_map, "[Grime]"));
}

/// Sound reaches a control through the one player the screen installed, wired by whatever built the control:
/// the dossier's buttons come from the widget factory, the map node builds its own. Each has to end up with
/// exactly one wiring -- none means the screen went silent, two means a second caller wired it as well.
DEFN_TEST(campaign_map_wires_every_control_to_the_installed_sfx_player_once) {
    const TreeMountedNode<MenuManager> menu_manager_owner;
    CampaignMapView *campaign_map = show_campaign_map(menu_manager_owner);

    DEFN_REQUIRE(campaign_map != nullptr);
    OperationDossierView *dossier = campaign_map->dossier();
    DEFN_REQUIRE(dossier != nullptr);
    auto *node_interaction = Object::cast_to<Button>(campaign_map->get_node_or_null("WideUI/MissionNodes/level_01/Interaction"));
    for (Button *button : {dossier->deploy_button(), dossier->back_button(), node_interaction}) {
        DEFN_REQUIRE(button != nullptr);
        DEFN_CHECK_EQ(button->get_signal_connection_list("mouse_entered").size(), static_cast<int64_t>(1));
        DEFN_CHECK_EQ(button->get_signal_connection_list("button_down").size(), static_cast<int64_t>(1));
    }
}

} // namespace defn
