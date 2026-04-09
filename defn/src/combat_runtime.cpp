#include "combat_runtime.h"

#include "animation_controller.h"
#include "attack_target_helpers.h"
#include "health_component.h"
#include "projectile_attack.h"
#include "unit.h"

#include <algorithm>
#include <limits>

#include <godot_cpp/core/object.hpp>

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
    if (try_keep_target()) {
        return;
    }

    find_new_target();
}

bool CombatRuntime::try_keep_target() {
    if (state_.target == nullptr || AttackTarget::is_dead(state_.target)) {
        return false;
    }

    const real_t distance = get_forward_distance(state_.target);
    if (distance < 0) {
        return false;
    }

    if (distance <= config_.attack_range) {
        if (state_.attack_mode != AttackMode::MELEE) {
            state_.attack_mode = AttackMode::MELEE;
            animation_->hide_muzzle_flash();
        }
        return true;
    }

    if (distance <= config_.ranged_range) {
        if (state_.attack_mode != AttackMode::RANGED) {
            state_.attack_mode = AttackMode::RANGED;
        }
        return true;
    }

    return false;
}

void CombatRuntime::find_new_target() {
    state_.target = nullptr;
    state_.engaged = false;

    Node2D *best_melee_target = nullptr;
    Node2D *best_ranged_target = nullptr;
    real_t closest_melee_distance = std::numeric_limits<real_t>::max();
    real_t closest_ranged_distance = std::numeric_limits<real_t>::max();

    TypedArray<Area2D> overlapping = detection_area_->get_overlapping_areas();
    for (const auto &area_variant : overlapping) {
        auto *hitbox = Object::cast_to<Area2D>(area_variant.operator Object *());
        if (hitbox == nullptr) {
            continue;
        }

        auto *other = AttackTarget::resolve(hitbox->get_parent());
        if (other == nullptr || AttackTarget::is_dead(other) || AttackTarget::get_side(other) == config_.side) {
            continue;
        }

        const real_t distance = get_forward_distance(other);
        if (distance < 0) {
            continue;
        }

        if (distance <= config_.attack_range && distance < closest_melee_distance) {
            closest_melee_distance = distance;
            best_melee_target = other;
        }
        if (distance <= config_.ranged_range && distance < closest_ranged_distance) {
            closest_ranged_distance = distance;
            best_ranged_target = other;
        }
    }

    animation_->hide_muzzle_flash();
    state_.attack_mode = AttackMode::NONE;

    if (best_melee_target != nullptr) {
        state_.target = best_melee_target;
        state_.engaged = true;
        state_.attack_mode = AttackMode::MELEE;
        return;
    }

    if (best_ranged_target != nullptr) {
        state_.target = best_ranged_target;
        state_.engaged = true;
        state_.attack_mode = AttackMode::RANGED;
    }
}

real_t CombatRuntime::get_forward_distance(Node2D *other) const {
    if (config_.side == UnitSide::FRIENDLY) {
        return other->get_global_position().x - unit_->get_global_position().x;
    }

    return unit_->get_global_position().x - other->get_global_position().x;
}

void CombatRuntime::try_spawn_pending_projectile() {
    if (!state_.pending_projectile.active || animation_ == nullptr || !animation_->consume_shoot_effect_triggered()) {
        return;
    }

    auto *projectile = memnew(ProjectileAttack);
    Node *projectile_parent = unit_ != nullptr ? unit_->get_parent() : nullptr;
    if (projectile_parent == nullptr) {
        projectile_parent = unit_;
    }

    if (projectile_parent != nullptr && config_.projectile_attack.has_value()) {
        Node2D *direct_target = nullptr;
        if (!state_.pending_projectile.target_id.is_null()) {
            direct_target = AttackTarget::resolve(ObjectDB::get_instance(static_cast<uint64_t>(state_.pending_projectile.target_id)));
        }

        projectile_parent->add_child(projectile);
        projectile->configure(*config_.projectile_attack, config_.side, config_.ranged_flash_color, animation_->get_muzzle_global_position(),
                              state_.pending_projectile.target_global_position, direct_target, config_.ranged_damage);
    }

    state_.pending_projectile = {};
}

void CombatRuntime::perform_behavior(double delta) {
    if (state_.pending_projectile.active) {
        unit_->set_velocity(Vector2(0, 0));
        return;
    }

    if (state_.engaged && state_.target != nullptr && !AttackTarget::is_dead(state_.target)) {
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
            trigger_attack();
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

void CombatRuntime::trigger_attack() {
    if (state_.target == nullptr) {
        return;
    }

    if (state_.attack_mode == AttackMode::MELEE) {
        animation_->play_attack_animation();
        AttackTarget::take_damage(state_.target, config_.melee_damage);
        AttackTarget::flash_damage(state_.target, config_.melee_flash_color);
        return;
    }

    if (state_.attack_mode == AttackMode::RANGED) {
        if (config_.projectile_attack.has_value()) {
            state_.pending_projectile = {
                .active = true,
                .target_id = ObjectID(state_.target->get_instance_id()),
                .target_global_position = AttackTarget::get_global_position(state_.target),
            };
            animation_->play_shoot_animation(false, config_.projectile_attack->spawn_animation_frame);
            try_spawn_pending_projectile();
            return;
        }

        animation_->play_shoot_animation();
        animation_->consume_shoot_effect_triggered();

        AttackTarget::take_damage(state_.target, config_.ranged_damage);
        AttackTarget::flash_damage(state_.target, config_.ranged_flash_color);
    }
}

} // namespace defn