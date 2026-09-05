// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef BATTLE_ENTITY_H
#define BATTLE_ENTITY_H

#include "attack_target.h"

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class HealthComponent;
class HitboxComponent;
class MovementComponent;

class BattleEntity : public Node2D, public AttackTarget {
    GDCLASS(BattleEntity, Node2D)

  public:
    [[nodiscard]] int take_damage(int amount) override;
    [[nodiscard]] bool is_dead() const override;
    [[nodiscard]] UnitSide get_side() const override { return side_; }
    [[nodiscard]] float get_threat_weight() const override { return threat_weight_; }
    [[nodiscard]] UnitRole get_role() const override { return role_; }
    [[nodiscard]] int get_current_health() const override { return get_current_hp(); }
    [[nodiscard]] Node2D *get_target_node() override { return target_anchor_ != nullptr ? target_anchor_ : this; }
    [[nodiscard]] const Node2D *get_target_node() const override { return target_anchor_ != nullptr ? target_anchor_ : this; }
    int get_current_hp() const;
    int get_max_hp() const;
    Area2D *get_hitbox() const;
    virtual MovementComponent *get_movement_component() const { return nullptr; }

  protected:
    static void _bind_methods();

    void set_side(UnitSide side) { side_ = side; }
    void set_threat_weight(float threat_weight) { threat_weight_ = threat_weight; }
    void set_role(UnitRole role) { role_ = role; }
    void set_health_component(HealthComponent *health_component) { health_component_ = health_component; }
    void set_hitbox_component(HitboxComponent *hitbox_component) { hitbox_component_ = hitbox_component; }
    HealthComponent *get_health_component() const { return health_component_; }
    HitboxComponent *get_hitbox_component() const { return hitbox_component_; }
    Node2D *ensure_target_anchor();
    Node2D *get_target_anchor() const { return target_anchor_; }

  private:
    UnitSide side_ = UnitSide::FRIENDLY;
    float threat_weight_ = 1.0F;
    UnitRole role_ = UnitRole::NONE;
    HealthComponent *health_component_ = nullptr;
    HitboxComponent *hitbox_component_ = nullptr;
    Node2D *target_anchor_ = nullptr;
};

} // namespace defn

#endif
