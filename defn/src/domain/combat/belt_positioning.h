// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef BELT_POSITIONING_H
#define BELT_POSITIONING_H

#include "combat_logic.h"
#include "unit_definition.h"

#include <map>
#include <span>
#include <vector>

namespace defn {

struct BeltUnitSnapshot {
    EntityId id;
    UnitSide side = UnitSide::FRIENDLY;
    Vector2 position;
    EntityId approach_id;
    EntityId target_id;
    Vector2 approach_position;
    AttackMode attack_mode = AttackMode::NONE;
    bool dead = false;
    bool manual = false;
    bool attacking = false;
    float attack_y_speed_scale = 1.0F;
    float speed = 0.0F;
    BeltPositioningConfig config;
};

struct BeltPositionResult {
    EntityId id;
    float next_y = 0.0F;
};

// One match owns one instance. Call once per frame with a complete, immutable foot-position snapshot.
class BeltPositioning {
  public:
    [[nodiscard]] std::vector<BeltPositionResult> advance(std::span<const BeltUnitSnapshot> units, float top, float bottom, double delta);
    void clear() { states_.clear(); }

  private:
    struct State {
        EntityId target;
        EntityId ranged_target;
        int slot = 0;
        bool assigned = false;
        bool moving = false;
        bool ranged_repositioning = false;
        float velocity = 0.0F;
    };
    void retain_assignments(std::span<const BeltUnitSnapshot> units, float top, float bottom);
    int choose_slot(const BeltUnitSnapshot &unit, std::span<const BeltUnitSnapshot> units, float top, float bottom) const;
    BeltPositionResult move_unit(const BeltUnitSnapshot &unit, std::span<const BeltUnitSnapshot> units, float top, float bottom, double delta);
    std::map<uint64_t, State> states_;
};

} // namespace defn

#endif
