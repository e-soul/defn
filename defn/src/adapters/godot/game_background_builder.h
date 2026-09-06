// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef GAME_BACKGROUND_BUILDER_H
#define GAME_BACKGROUND_BUILDER_H

#include "gameplay_rules.h"

#include <godot_cpp/classes/parallax2d.hpp>
#include <godot_cpp/variant/string.hpp>

namespace defn {

using namespace godot;

// The ground the match is fought on. `Parallax2D` tiles its child forever once `repeat_size` is set, so the belt has
// no far edge to build up to; the only question left is how many copies have to be drawn to keep the screen covered.
class GameBackgroundBuilder {
  public:
    GameBackgroundBuilder() = delete;

    // Copies drawn either side of the one under the camera. Two would already cover a screen-wide tile; a third
    // absorbs a wider viewport or a narrower texture without leaving a gap at the edge.
    static constexpr int REPEAT_MARGIN = 2;

    static Parallax2D *build(const String &background_path, const GameplayRules &rules);
};

} // namespace defn

#endif