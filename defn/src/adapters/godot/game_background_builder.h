#ifndef GAME_BACKGROUND_BUILDER_H
#define GAME_BACKGROUND_BUILDER_H

#include "gameplay_rules.h"

#include <godot_cpp/classes/parallax2d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string.hpp>

namespace defn {

using namespace godot;

struct GameBackgroundBuildResult {
    Parallax2D *background = nullptr;
    real_t world_width = 0.0F;
};

class GameBackgroundBuilder {
  public:
    GameBackgroundBuilder() = delete;

    static GameBackgroundBuildResult build(const String &background_path, const GameplayRules &rules);
};

} // namespace defn

#endif