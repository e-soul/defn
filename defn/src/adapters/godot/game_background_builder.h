// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef GAME_BACKGROUND_BUILDER_H
#define GAME_BACKGROUND_BUILDER_H

#include "background_layer.h"
#include "gameplay_rules.h"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/parallax2d.hpp>
#include <godot_cpp/variant/string.hpp>

#include <vector>

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

    // The layered form: one `Parallax2D` per plane, each with its own scroll rate, added back to front under a
    // single node so the caller still attaches one child. A layer whose texture will not load is skipped rather
    // than failing the stack -- a missing sky is a worse-looking match, a missing background is no match at all.
    static Node2D *build_stack(const std::vector<BackgroundLayer> &layers, const GameplayRules &rules);
};

} // namespace defn

#endif