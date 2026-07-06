#include "combat_attack_executor.h"

#include "animation_controller.h"
#include "attack_target_resolver.h"
#include "battle_entity.h"
#include "projectile_attack.h"

namespace defn {

namespace {

Color to_godot_color(const CombatColor &color) { return {color.r, color.g, color.b, color.a}; }

} // namespace

void CombatAttackExecutor::spawn_pending_projectile(const CombatConfig &config, BattleEntity *unit, AnimationController *animation,
                                                    PendingProjectileSpawn &pending_projectile) {
    if (!pending_projectile.active) {
        return;
    }

    if (animation != nullptr && !animation->consume_shoot_effect_triggered()) {
        return;
    }

    auto *projectile = memnew(ProjectileAttack);
    Node *projectile_parent = unit != nullptr ? unit->get_parent() : nullptr;
    if (projectile_parent == nullptr) {
        projectile_parent = unit;
    }

    if (projectile_parent != nullptr && config.projectile_attack.has_value()) {
        AttackTarget *direct_target = resolve_attack_target(pending_projectile.target_id);
        Vector2 launch_position;
        if (animation != nullptr) {
            launch_position = animation->get_muzzle_global_position();
        } else if (unit != nullptr) {
            launch_position = unit->get_target_global_position();
        }

        projectile_parent->add_child(projectile);
        projectile->configure(*config.projectile_attack, config.side, to_godot_color(config.ranged_flash_color), launch_position,
                              pending_projectile.target_global_position, direct_target, config.ranged_damage);
    }

    pending_projectile = {};
}

void CombatAttackExecutor::trigger_attack(const CombatConfig &config, AttackMode attack_mode, AttackTarget *target, AnimationController *animation,
                                          PendingProjectileSpawn &pending_projectile) {
    if (target == nullptr) {
        return;
    }

    if (attack_mode == AttackMode::MELEE) {
        if (animation != nullptr) {
            animation->play_attack_animation();
        }
        target->take_damage(config.melee_damage);
        target->flash_damage(to_godot_color(config.melee_flash_color));
        return;
    }

    if (attack_mode != AttackMode::RANGED) {
        return;
    }

    if (config.projectile_attack.has_value()) {
        pending_projectile = {
            .active = true,
            .target_id = target->get_target_object_id(),
            .target_global_position = target->get_target_global_position(),
        };
        if (animation != nullptr) {
            animation->play_shoot_animation(false, config.projectile_attack->spawn_animation_frame);
        }
        return;
    }

    if (animation != nullptr) {
        animation->play_shoot_animation();
        animation->consume_shoot_effect_triggered();
    }
    target->take_damage(config.ranged_damage);
    target->flash_damage(to_godot_color(config.ranged_flash_color));
}

} // namespace defn