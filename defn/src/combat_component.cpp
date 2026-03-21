#include "combat_component.h"
#include "animation_controller.h"
#include "grid_manager.h"
#include "health_component.h"
#include "unit.h"
#include <godot_cpp/classes/scene_tree.hpp>

namespace defn {

void CombatComponent::_bind_methods() {}

void CombatComponent::configure(Unit *p_unit, HealthComponent *p_health, AnimationController *p_anim, const Config &cfg) {
    unit = p_unit;
    health = p_health;
    anim = p_anim;
    config = cfg;
    attack_cooldown = 0.0;
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

double CombatComponent::get_forward_distance(Unit *other) const {
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

    double dist = get_forward_distance(target);
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

    String target_group = (config.side == UnitSide::FRIENDLY) ? "hostiles" : "friendlies";

    Unit *best_melee = nullptr;
    Unit *best_ranged = nullptr;
    double closest_melee = 1e9;
    double closest_ranged = 1e9;

    TypedArray<Node> targets = unit->get_tree()->get_nodes_in_group(target_group);
    for (const auto &node_variant : targets) {
        auto *other = Object::cast_to<Unit>(node_variant.operator Object *());
        if (!other || other->is_dead()) {
            continue;
        }

        double dist = get_forward_distance(other);
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
    if (attack_cooldown > 0.0) {
        attack_cooldown -= delta;
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
                anim->set_anim_state(AnimState::ATTACK);
                anim->get_sprite()->set_frame_and_progress(0, 0.0);
                anim->get_sprite()->stop();
            } else if (attack_mode == AttackMode::RANGED) {
                anim->set_anim_state(AnimState::SHOOT);
                anim->get_sprite()->set_frame_and_progress(0, 0.0);
                anim->get_sprite()->stop();
            }
        }

        if (attack_mode == AttackMode::MELEE && attack_cooldown <= 0.0) {
            attack_cooldown = 1.0 / config.melee_attack_speed;
            anim->set_anim_state(AnimState::ATTACK);
            anim->get_sprite()->play("attack");
            anim->get_sprite()->set_frame_and_progress(0, 0.0);
            target->take_damage(config.melee_damage);
            target->flash_damage(config.melee_flash_color);
        } else if (attack_mode == AttackMode::RANGED && attack_cooldown <= 0.0) {
            attack_cooldown = 1.0 / config.ranged_attack_speed;
            anim->set_anim_state(AnimState::SHOOT);
            anim->get_sprite()->play("shoot");
            anim->get_sprite()->set_frame_and_progress(0, 0.0);
            target->take_damage(config.ranged_damage);
            target->flash_damage(config.ranged_flash_color);
            anim->play_muzzle_flash();
        }
    } else {
        engaged = false;
        target = nullptr;
        anim->hide_muzzle_flash();
        attack_mode = AttackMode::NONE;

        auto current_anim = anim->get_anim_state();
        if (current_anim == AnimState::ATTACK || current_anim == AnimState::SHOOT) {
            anim->set_anim_state(AnimState::WALK);
        }
        unit->do_movement(delta);
    }
}

} // namespace defn
