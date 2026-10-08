// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "deploy_tray.h"
#include "hud.h"
#include "match_presentation.h"
#include "pause_menu.h"
#include "ui_sfx_player.h"
#include "ui_test_helpers.h"
#include "ui_theme_provider.h"
#include <algorithm>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/core/memory.hpp>
#include <string>
#include <vector>

namespace defn {
using namespace ui_test;
namespace {
AudioStreamPlayer *deploy_sound(UiSfxPlayer *sfx) {
    DEFN_REQUIRE(sfx != nullptr);
    auto *player = Object::cast_to<AudioStreamPlayer>(sfx->get_node_or_null("DeployCardSfxPlayer"));
    DEFN_REQUIRE(player != nullptr);
    DEFN_REQUIRE(player->get_stream().is_valid());
    return player;
}
void check_deploy_card_audio(bool desktop) {
    (void)UiThemeProvider::resolve_profile(desktop ? UiProfile::Standard : UiProfile::Small, !desktop);
    const TreeMountedNode<Control> owner;
    auto *sfx = UiSfxPlayer::install(owner.get());
    auto *player = deploy_sound(sfx);
    auto *tray = memnew(DeployTray);
    owner.get()->add_child(tray);
    auto *card = tray->add_card({.unit_id = "breacher", .title = "Breacher", .cost = 20});
    tray->apply_appearance(UiThemeProvider::appearance(desktop ? UiThemeContext::DesktopMatch : UiThemeContext::Default));
    tray->apply_layout({.tray = {.width = 500, .height = 110}, .desktop_reference = desktop});
    const godot::Vector2 point = card->get_global_rect().get_center() - tray->get_global_position();
    const auto button_event = [](godot::Vector2 position, bool pressed) {
        Ref<InputEventMouseButton> event;
        event.instantiate();
        event->set_button_index(MOUSE_BUTTON_LEFT);
        event->set_position(position);
        event->set_pressed(pressed);
        return event;
    };
    const auto release = [&](godot::Vector2 position) { tray->_input(button_event(tray->get_global_transform_with_canvas().xform(position), false)); };

    tray->_gui_input(button_event(point, true));
    DEFN_CHECK(!player->is_playing());
    release(point);
    DEFN_CHECK(player->is_playing());
    player->stop();

    tray->_gui_input(button_event(point, true));
    Ref<InputEventMouseMotion> motion;
    motion.instantiate();
    const godot::Vector2 dragged = point + godot::Vector2(80, 0);
    motion->set_position(tray->get_global_transform_with_canvas().xform(dragged));
    tray->_input(motion);
    release(dragged);
    DEFN_CHECK(!player->is_playing());

    tray->_gui_input(button_event(point, true));
    tray->apply_appearance(UiThemeProvider::appearance(desktop ? UiThemeContext::DesktopMatch : UiThemeContext::Default));
    tray->apply_layout({.tray = {.width = 500, .height = 110}, .desktop_reference = desktop});
    release(point);
    DEFN_CHECK(!player->is_playing());

    card->set_disabled(true);
    tray->_gui_input(button_event(point, true));
    release(point);
    DEFN_CHECK(!player->is_playing());
}
} // namespace

DEFN_TEST(deploy_card_audio_plays_for_an_accepted_tap_but_not_a_swipe_or_disabled_card) {
    UiThemeProvider::reload();
    check_deploy_card_audio(false);
    check_deploy_card_audio(true);
    UiThemeProvider::reload();
}

DEFN_TEST(match_presentation_keeps_world_and_ui_ownership_separate_across_resize) {
    TreeMountedNode<MatchPresentation> mounted;
    auto *presentation = mounted.get();
    auto *world = Object::cast_to<SubViewport>(presentation->get_node_or_null("BattlefieldHost/Battlefield"));
    DEFN_REQUIRE(world != nullptr);
    auto *hud = Object::cast_to<HUD>(presentation->get_node_or_null("FullScreenUI/HUD"));
    DEFN_REQUIRE(hud != nullptr);
    DEFN_CHECK(world->get_node_or_null("GameManager/HUD") == nullptr);
    DEFN_CHECK_EQ(world->get_size_2d_override(), Vector2i(1920, 1080));
    DEFN_CHECK(world->is_audio_listener_2d());
    DEFN_CHECK_EQ(world->get_node_or_null("GameManager")->get_process_mode(), Node::PROCESS_MODE_PAUSABLE);
    auto *pause_menu = Object::cast_to<PauseMenu>(presentation->get_node_or_null("FullScreenUI/PauseMenu"));
    DEFN_REQUIRE(pause_menu != nullptr);
    const auto game_id = world->get_node_or_null("GameManager")->get_instance_id();
    const auto hud_id = hud->get_instance_id();
    presentation->set_size({390, 844});
    presentation->apply_snapshot({.content = {.width = 390, .height = 844}, .touch = true, .fine_pointer = false});
    pause_menu->toggle_pause();
    presentation->set_size({667, 330});
    presentation->apply_snapshot({.content = {.width = 667, .height = 330}, .touch = true, .fine_pointer = false});
    DEFN_CHECK_EQ(world->get_node_or_null("GameManager")->get_instance_id(), game_id);
    DEFN_CHECK_EQ(hud->get_instance_id(), hud_id);
    DEFN_CHECK_EQ(world->get_size_2d_override(), Vector2i(1920, 1080));
    DEFN_CHECK(world->is_audio_listener_2d());
    DEFN_CHECK(Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop())->is_paused());
    pause_menu->toggle_pause();
}

DEFN_TEST(hud_level_text_reserves_visible_width_after_load_and_theme_round_trip) {
    const TreeMountedNode<HUD> owner;
    auto *hud = owner.get();
    hud->set_level("DESERT OUTPOST");
    auto *level = find_label_by_text(hud, "DESERT OUTPOST");
    DEFN_REQUIRE(level != nullptr);
    hud->apply_appearance(UiThemeProvider::appearance(UiThemeContext::Default));
    DEFN_CHECK(level->get_custom_minimum_size().x > 0);
    const auto before = hud->measure_layout(390);
    hud->update_core_resource(41);
    const auto after = hud->measure_layout(390);
    DEFN_CHECK_EQ(before.flow_height, after.flow_height);
    hud->apply_appearance(UiThemeProvider::appearance(UiThemeContext::PhoneLandscape));
    hud->apply_appearance(UiThemeProvider::appearance(UiThemeContext::Default));
    DEFN_CHECK(level->get_custom_minimum_size().x > 0);
}
} // namespace defn
