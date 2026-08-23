// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_sfx_player.h"

#include "godot_string.h"
#include "ui_theme_models.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>

namespace defn {

void UiSfxPlayer::_bind_methods() {}

namespace {

/// Held as an instance id rather than a pointer: a scene change frees the player that set it, and a raw pointer
/// would outlive it.
uint64_t g_active_id = 0;

} // namespace

UiSfxPlayer *UiSfxPlayer::active() {
    if (g_active_id == 0) {
        return nullptr;
    }
    auto *player = godot::Object::cast_to<UiSfxPlayer>(godot::UtilityFunctions::instance_from_id(static_cast<int64_t>(g_active_id)));
    return player != nullptr && player->is_inside_tree() ? player : nullptr;
}

void UiSfxPlayer::configure(const UiSfxData &config) {
    hover_player_ = create_player("UiHoverSfxPlayer", config.hover);
    click_player_ = create_player("UiClickSfxPlayer", config.click);
    deploy_card_player_ = create_player("DeployCardSfxPlayer", config.deploy_card);
    g_active_id = get_instance_id();
}

void UiSfxPlayer::connect_button(godot::BaseButton *button, std::string_view press_role) {
    if (button == nullptr) {
        return;
    }
    if (hover_player_ != nullptr) {
        button->connect("mouse_entered", callable_mp(this, &UiSfxPlayer::play_hover).bind(button));
    }
    const bool deploy_card = press_role == "deploy_card";
    if ((deploy_card ? deploy_card_player_ : click_player_) != nullptr) {
        button->connect("button_down", callable_mp(this, &UiSfxPlayer::play_press).bind(button, deploy_card));
    }
}

godot::AudioStreamPlayer *UiSfxPlayer::create_player(const char *name, const UiSoundData &sound) {
    if (sound.path.empty()) {
        return nullptr;
    }

    godot::Ref<godot::AudioStream> stream = godot::ResourceLoader::get_singleton()->load(to_godot_string(sound.path));
    if (stream.is_null()) {
        godot::UtilityFunctions::printerr("UiSfxPlayer: Failed to load ", to_godot_string(sound.path));
        return nullptr;
    }

    auto *player = memnew(godot::AudioStreamPlayer);
    player->set_name(name);
    player->set_stream(stream);
    player->set_volume_db(static_cast<float>(godot::UtilityFunctions::linear_to_db(std::max(sound.volume_linear, 0.0001F))));
    add_child(player);
    return player;
}

void UiSfxPlayer::play_hover(godot::BaseButton *button) {
    if (button != nullptr && !button->is_disabled() && hover_player_ != nullptr && hover_player_->is_inside_tree()) {
        hover_player_->play();
    }
}

void UiSfxPlayer::play_press(godot::BaseButton *button, bool deploy_card) {
    godot::AudioStreamPlayer *player = deploy_card ? deploy_card_player_ : click_player_;
    if (button != nullptr && !button->is_disabled() && player != nullptr && player->is_inside_tree()) {
        player->play();
    }
}

} // namespace defn
