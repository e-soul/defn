// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef BELT_POSITIONING_RUNTIME_H
#define BELT_POSITIONING_RUNTIME_H

#include "belt_positioning.h"

#include <map>
#include <span>

namespace defn {

class Unit;

// Collects Godot facts, then applies one shared domain positioning result.
class BeltPositioningRuntime {
  public:
    void step(std::span<Unit *const> units, float top, float bottom, double delta);
    void clear();

  private:
    BeltPositioning solver_;
    std::map<uint64_t, uint64_t> spawn_order_;
    uint64_t next_order_ = 1;
};

} // namespace defn

#endif
