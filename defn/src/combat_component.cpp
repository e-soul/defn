#include "combat_component.h"
#include "animation_controller.h"
#include "grid_manager.h"
#include "health_component.h"
#include "unit.h"

#include <algorithm>
#include <limits>

namespace defn {

void CombatComponent::_bind_methods() {}

void CombatComponent::configure(Unit *p_unit, HealthComponent *p_health, AnimationController *p_anim, Area2D *p_detection_area, const Config &cfg) {
    unit = p_unit;
    health = p_health;
    anim = p_anim;
    detection_area = p_detection_area;
    config = cfg;
    attack_cooldown_seconds = 0.0;
}

void CombatComponent::_process(double delta) {
    if (health->is_dead()) {
        return;
    }

    if (config.side == UnitSide::HOSTILE) {
        check_breach();
    }

    update_cooldowns(delta);
    find_target();
    perform_behavior(delta);
}

// --- Targeting ---

real_t CombatComponent::get_forward_distance(Unit *other) const {
    if (config.side == UnitSide::FRIENDLY) {
        return other->get_position().x - unit->get_position().x;
    }
    return unit->get_position().x - other->get_position().x;
}

void CombatComponent::find_target() {
    if (try_keep_target()) {
        return;
    }
    find_new_target();
}

bool CombatComponent::try_keep_target() {
    if (!target || target->is_dead()) {
        return false;
    }

    const real_t dist = get_forward_distance(target);
    if (dist < 0) {
        return false;
    }

    if (dist <= config.attack_range) {
        if (attack_mode != AttackMode::MELEE) {
            attack_mode = AttackMode::MELEE;
            anim->hide_muzzle_flash();
        }
        return true;
    }

    if (dist <= config.ranged_range) {
        if (attack_mode != AttackMode::RANGED) {
            attack_mode = AttackMode::RANGED;
        }
        return true;
    }

    return false;
}

void CombatComponent::find_new_target() {
    target = nullptr;
    engaged = false;

    Unit *best_melee = nullptr;
    Unit *best_ranged = nullptr;
    real_t closest_melee = std::numeric_limits<real_t>::max();
    real_t closest_ranged = std::numeric_limits<real_t>::max();

    TypedArray<Area2D> overlapping = detection_area->get_overlapping_areas();
    for (const auto &area_variant : overlapping) {
        auto *hitbox = Object::cast_to<Area2D>(area_variant.operator Object *());
        if (!hitbox) {
            continue;
        }
        auto *other = Object::cast_to<Unit>(hitbox->get_parent());
        if (!other || other->is_dead()) {
            continue;
        }

        const real_t dist = get_forward_distance(other);
        if (dist < 0) {
            continue;
        }

        if (dist <= config.attack_range && dist < closest_melee) {
            closest_melee = dist;
            best_melee = other;
        }
        if (dist <= config.ranged_range && dist < closest_ranged) {
            closest_ranged = dist;
            best_ranged = other;
        }
    }

    anim->hide_muzzle_flash();
    attack_mode = AttackMode::NONE;

    if (best_melee) {
        target = best_melee;
        engaged = true;
        attack_mode = AttackMode::MELEE;
    } else if (best_ranged) {
        target = best_ranged;
        engaged = true;
        attack_mode = AttackMode::RANGED;
    }
}

// --- Combat ---

void CombatComponent::update_cooldowns(double delta) {
    if (attack_cooldown_seconds > 0.0) {
        attack_cooldown_seconds = std::max(attack_cooldown_seconds - delta, 0.0);
    }
}

void CombatComponent::check_breach() {
    if (unit->get_position().x < GridManager::BREACH_X) {
        unit->notify_breach();
    }
}

void CombatComponent::perform_behavior(double delta) {
    if (engaged && target && !target->is_dead()) {
        unit->set_velocity(Vector2(0, 0));

        auto current_anim = anim->get_anim_state();
        bool needs_pose_update = (current_anim == AnimState::WALK) || (attack_mode == AttackMode::MELEE && current_anim != AnimState::ATTACK) ||
                                 (attack_mode == AttackMode::RANGED && current_anim != AnimState::SHOOT);

        if (needs_pose_update) {
            if (attack_mode == AttackMode::MELEE) {
                anim->hold_anim_state(AnimState::ATTACK);
            } else if (attack_mode == AttackMode::RANGED) {
                anim->hold_anim_state(AnimState::SHOOT);
            }
        }

        const double attack_period_seconds = get_attack_period_seconds();
        if (attack_period_seconds > 0.0 && attack_cooldown_seconds <= 0.0) {
            trigger_attack();
            attack_cooldown_seconds = attack_period_seconds;
        }
    } else {
        engaged = false;
        target = nullptr;
        anim->hide_muzzle_flash();
        attack_mode = AttackMode::NONE;
        attack_cooldown_seconds = 0.0;

        auto current_anim = anim->get_anim_state();
        if (current_anim == AnimState::ATTACK || current_anim == AnimState::SHOOT) {
            anim->set_anim_state(AnimState::WALK);
        }
        unit->do_movement(delta);
    }
}

double CombatComponent::get_attack_period_seconds() const {
    switch (attack_mode) {
    case AttackMode::MELEE:
        return config.melee_attack_period_seconds;
    case AttackMode::RANGED:
        return config.ranged_attack_period_seconds;
    case AttackMode::NONE:
        return 0.0;
    }

    return 0.0;
}

void CombatComponent::trigger_attack() {
    if (!target) {
        return;
    }

    if (attack_mode == AttackMode::MELEE) {
        anim->play_attack_animation();
        target->take_damage(config.melee_damage);
        target->flash_damage(config.melee_flash_color);
        return;
    }

    if (attack_mode == AttackMode::RANGED) {
        anim->play_shoot_animation();
        target->take_damage(config.ranged_damage);
        target->flash_damage(config.ranged_flash_color);
    }
}

} // namespace defn
