// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UNIT_CONTROL_COMPONENT_H
#define UNIT_CONTROL_COMPONENT_H

#include "reposition_logic.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class AnimationController;
class CombatComponent;
class MovementComponent;
class Unit;

class UnitControlComponent : public Node {
    GDCLASS(UnitControlComponent, Node)

  public:
    void configure(Unit *unit, MovementComponent *movement, AnimationController *animation, CombatComponent *combat);
    [[nodiscard]] bool request_reposition(real_t destination_x, float arrival_epsilon);
    void cancel_without_combat_resume();
    [[nodiscard]] bool is_repositioning() const { return state_.mode == UnitControlMode::REPOSITIONING; }

    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    void apply_request_intents(const RepositionIntents &intents);
    void finish_reposition(const RepositionStep &step);

    Unit *unit_ = nullptr;
    MovementComponent *movement_ = nullptr;
    AnimationController *animation_ = nullptr;
    CombatComponent *combat_ = nullptr;
    RepositionState state_{};
    float arrival_epsilon_ = REPOSITION_ARRIVAL_EPSILON;
    bool canceled_ = false;
};

} // namespace defn

#endif
