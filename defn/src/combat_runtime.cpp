#include "combat_runtime.h"

#include "animation_controller.h"
#include "combat_attack_executor.h"
#include "combat_target_selector.h"
#include "health_component.h"
#include "unit.h"

#include <algorithm>

namespace defn {

void CombatRuntime::configure(Unit *unit, HealthComponent *health, AnimationController *animation, Area2D *detection_area, const CombatConfig &config) {
    unit_ = unit;
    health_ = health;
    animation_ = animation;
    detection_area_ = detection_area;
    config_ = config;
    selection_ = {};
    state_ = {};
    pending_projectile_ = {};
}

void CombatRuntime::update(double delta) {
    if (health_ == nullptr || health_->is_dead()) {
        return;
    }

    try_spawn_pending_projectile();
    update_target();

    CombatLogicInput input;
    input.state = state_;
    input.selection = selection_;
    input.current_pose = animation_ != nullptr ? map_pose_state(animation_->get_anim_state()) : CombatPoseState::OTHER;
    input.delta = delta;
    input.unit_dead = health_->is_dead();
    input.projectile_pending = pending_projectile_.active;

    apply_logic_step(advance_combat_logic(config_, input), delta);
}

void CombatRuntime::update_target() { selection_ = CombatTargetSelector::select(unit_, detection_area_, config_, state_.target); }

void CombatRuntime::try_spawn_pending_projectile() { CombatAttackExecutor::spawn_pending_projectile(config_, unit_, animation_, pending_projectile_); }

void CombatRuntime::apply_logic_step(const CombatLogicStep &step, double delta) {
    state_ = step.state;

    if (unit_ == nullptr) {
        return;
    }

    if (animation_ != nullptr) {
        if (step.intent.hide_muzzle_flash) {
            animation_->hide_muzzle_flash();
        }

        switch (step.intent.pose) {
        case CombatPoseIntent::WALK:
            animation_->set_anim_state(AnimState::WALK);
            break;
        case CombatPoseIntent::ATTACK:
            animation_->hold_anim_state(AnimState::ATTACK);
            break;
        case CombatPoseIntent::SHOOT:
            animation_->hold_anim_state(AnimState::SHOOT);
            break;
        case CombatPoseIntent::NONE:
            break;
        }
    }

    if (step.intent.movement == CombatMovementIntent::STOP) {
        unit_->set_velocity(Vector2(0, 0));
    } else if (step.intent.movement == CombatMovementIntent::MOVE) {
        unit_->do_movement(delta);
    }

    if (step.intent.trigger_attack) {
        CombatAttackExecutor::trigger_attack(config_, state_.attack_mode, state_.target, animation_, pending_projectile_);
        try_spawn_pending_projectile();
    }
}

CombatPoseState CombatRuntime::map_pose_state(AnimState state) {
    switch (state) {
    case AnimState::WALK:
        return CombatPoseState::WALK;
    case AnimState::ATTACK:
        return CombatPoseState::ATTACK;
    case AnimState::SHOOT:
        return CombatPoseState::SHOOT;
    case AnimState::DEATH:
        return CombatPoseState::OTHER;
    }

    return CombatPoseState::OTHER;
}

} // namespace defn