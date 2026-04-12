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
    state_ = {};
}

void CombatRuntime::update(double delta) {
    if (health_ == nullptr || health_->is_dead()) {
        return;
    }

    try_spawn_pending_projectile();
    update_cooldowns(delta);
    update_target();
    perform_behavior(delta);
}

void CombatRuntime::update_cooldowns(double delta) {
    if (state_.attack_cooldown_seconds > 0.0) {
        state_.attack_cooldown_seconds = std::max(state_.attack_cooldown_seconds - delta, 0.0);
    }
}

void CombatRuntime::update_target() {
    const CombatTargetSelection selection = CombatTargetSelector::select(unit_, detection_area_, config_, state_.target);
    const bool mode_changed = selection.attack_mode != state_.attack_mode;

    state_.engaged = selection.engaged;
    state_.target = selection.target;
    state_.attack_mode = selection.attack_mode;

    if (mode_changed && selection.attack_mode != AttackMode::RANGED && animation_ != nullptr) {
        animation_->hide_muzzle_flash();
    }
}

void CombatRuntime::try_spawn_pending_projectile() { CombatAttackExecutor::spawn_pending_projectile(config_, unit_, animation_, state_.pending_projectile); }

void CombatRuntime::perform_behavior(double delta) {
    if (state_.pending_projectile.active) {
        unit_->set_velocity(Vector2(0, 0));
        return;
    }

    if (state_.engaged && state_.target != nullptr && !state_.target->is_dead()) {
        unit_->set_velocity(Vector2(0, 0));

        const auto current_anim = animation_->get_anim_state();
        const bool needs_pose_update = (current_anim == AnimState::WALK) || (state_.attack_mode == AttackMode::MELEE && current_anim != AnimState::ATTACK) ||
                                       (state_.attack_mode == AttackMode::RANGED && current_anim != AnimState::SHOOT);

        if (needs_pose_update) {
            if (state_.attack_mode == AttackMode::MELEE) {
                animation_->hold_anim_state(AnimState::ATTACK);
            } else if (state_.attack_mode == AttackMode::RANGED) {
                animation_->hold_anim_state(AnimState::SHOOT);
            }
        }

        const double attack_period_seconds = get_attack_period_seconds();
        if (attack_period_seconds > 0.0 && state_.attack_cooldown_seconds <= 0.0) {
            CombatAttackExecutor::trigger_attack(config_, state_.attack_mode, state_.target, animation_, state_.pending_projectile);
            try_spawn_pending_projectile();
            state_.attack_cooldown_seconds = attack_period_seconds;
        }
        return;
    }

    reset_engagement();

    const auto current_anim = animation_->get_anim_state();
    if (current_anim == AnimState::ATTACK || current_anim == AnimState::SHOOT) {
        animation_->set_anim_state(AnimState::WALK);
    }
    unit_->do_movement(delta);
}

void CombatRuntime::reset_engagement() {
    state_.engaged = false;
    state_.target = nullptr;
    animation_->hide_muzzle_flash();
    state_.attack_mode = AttackMode::NONE;
    state_.attack_cooldown_seconds = 0.0;
}

double CombatRuntime::get_attack_period_seconds() const {
    switch (state_.attack_mode) {
    case AttackMode::MELEE:
        return config_.melee_attack_period_seconds;
    case AttackMode::RANGED:
        return config_.ranged_attack_period_seconds;
    case AttackMode::NONE:
        return 0.0;
    }

    return 0.0;
}

} // namespace defn