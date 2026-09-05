// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ATTACK_TARGET_H
#define ATTACK_TARGET_H

#include "content_values.h"
#include "unit_side.h"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/color.hpp>

namespace defn {

using namespace godot;

class AttackTarget {
  public:
    virtual ~AttackTarget() = default;

    [[nodiscard]] virtual bool is_dead() const = 0;
    [[nodiscard]] virtual UnitSide get_side() const = 0;
    // Read by target selection, not by damage: how hard this target pulls fire, and how much of it is left.
    [[nodiscard]] virtual float get_threat_weight() const = 0;
    // What kind of thing this is, for a shooter that prefers kinds. Defaulted rather than pure: a target that never
    // says advertises NONE, which nothing prefers, so an implementer that ignores roles behaves exactly as before.
    [[nodiscard]] virtual UnitRole get_role() const { return UnitRole::NONE; }
    [[nodiscard]] virtual int get_current_health() const = 0;
    [[nodiscard]] virtual int take_damage(int amount) = 0;
    virtual void flash_damage(const godot::Color &color) = 0;
    [[nodiscard]] virtual Node2D *get_target_node() = 0;
    [[nodiscard]] virtual const Node2D *get_target_node() const = 0;

    ObjectID get_target_object_id() const {
        const Node2D *node = get_target_node();
        return node != nullptr ? ObjectID(node->get_instance_id()) : ObjectID();
    }

    godot::Vector2 get_target_global_position() const {
        const Node2D *node = get_target_node();
        return node != nullptr ? node->get_global_position() : godot::Vector2();
    }
};

} // namespace defn

#endif
