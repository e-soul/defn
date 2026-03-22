#include "sound_controller.h"
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

namespace defn {

void SoundController::_bind_methods() {}

void SoundController::configure(Node *owner_node, const UnitConfig &cfg) {
    if (cfg.shoot_sfx.path.is_empty()) {
        return;
    }

    shoot_player = memnew(AudioStreamPlayer2D);
    shoot_player->set_name("ShootSfxPlayer");

    auto *loader = ResourceLoader::get_singleton();
    Ref<AudioStream> stream = loader->load(cfg.shoot_sfx.path);
    if (stream.is_valid()) {
        shoot_player->set_stream(stream);
    }

    owner_node->add_child(shoot_player);
}

void SoundController::play_shoot_sfx() {
    if (shoot_player && shoot_player->get_stream().is_valid()) {
        shoot_player->play();
    }
}

} // namespace defn