#include "sound_controller.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

float linear_to_db(float linear) {
    const float clamped_linear = std::clamp(linear, 0.0001F, 1.0F);
    return 20.0F * std::log10(clamped_linear);
}

} // namespace

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

    shoot_player->set_volume_db(linear_to_db(cfg.shoot_sfx.volume_linear));

    const float variance = cfg.shoot_sfx.pitch_variance;
    const float randomized_pitch = cfg.shoot_sfx.pitch_scale + static_cast<float>(UtilityFunctions::randf_range(-variance, variance));
    shoot_player->set_pitch_scale(randomized_pitch > 0.01F ? randomized_pitch : 0.01F);

    owner_node->add_child(shoot_player);
}

void SoundController::play_shoot_sfx() {
    if (shoot_player && shoot_player->get_stream().is_valid()) {
        shoot_player->play();
    }
}

} // namespace defn