#ifndef SOUND_CONTROLLER_H
#define SOUND_CONTROLLER_H

#include "unit_definition.h"
#include <godot_cpp/classes/audio_stream_player2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class SoundController : public Node {
    GDCLASS(SoundController, Node)

  public:
    void configure(Node *owner_node, const UnitConfig &cfg);
    void play_shoot_sfx();

  protected:
    static void _bind_methods();

  private:
    AudioStreamPlayer2D *shoot_player = nullptr;
};

} // namespace defn

#endif