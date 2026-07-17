#include "ui_sfx_player.h"

#include "godot_string.h"
#include "menu_models.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>

namespace defn {

void UiSfxPlayer::_bind_methods() {}

void UiSfxPlayer::configure(const UiSfxData &config) {
    hover_player_ = create_player("UiHoverSfxPlayer", config.hover);
    click_player_ = create_player("UiClickSfxPlayer", config.click);
    deploy_card_player_ = create_player("DeployCardSfxPlayer", config.deploy_card);
}

void UiSfxPlayer::connect_menu_button(godot::BaseButton *button) {
    if (button == nullptr) {
        return;
    }
    if (hover_player_ != nullptr) {
        button->connect("mouse_entered", callable_mp(this, &UiSfxPlayer::play_hover).bind(button));
    }
    if (click_player_ != nullptr) {
        button->connect("pressed", callable_mp(this, &UiSfxPlayer::play_click).bind(button));
    }
}

void UiSfxPlayer::connect_deploy_card(godot::BaseButton *button) {
    if (button != nullptr && deploy_card_player_ != nullptr) {
        button->connect("pressed", callable_mp(this, &UiSfxPlayer::play_deploy_card).bind(button));
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
    if (button != nullptr && !button->is_disabled() && hover_player_ != nullptr) {
        hover_player_->play();
    }
}

void UiSfxPlayer::play_click(godot::BaseButton *button) {
    if (button != nullptr && !button->is_disabled() && click_player_ != nullptr) {
        click_player_->play();
    }
}

void UiSfxPlayer::play_deploy_card(godot::BaseButton *button) {
    if (button != nullptr && !button->is_disabled() && deploy_card_player_ != nullptr) {
        deploy_card_player_->play();
    }
}

} // namespace defn
