#include "combat_attack_executor.h"

#include "animation_controller.h"
#include "attack_target.h"
#include "attack_target_resolver.h"
#include "battle_entity.h"
#include "damage_dispatcher.h"
#include "godot_color.h"
#include "godot_vector.h"
#include "projectile_factory.h"

namespace defn {

namespace {

AttackTarget *resolve_entity_id(EntityId entity_id) { return entity_id.is_valid() ? resolve_attack_target(ObjectID(entity_id.value)) : nullptr; }

void apply_effect(const CombatCommand &command, const std::optional<ProjectileAttackConfig> &projectile_config, AnimationController *animation,
                  PendingProjectileSpawn &pending_projectile) {
    switch (command.effect) {
    case CombatEffectType::MELEE_ATTACK:
        if (animation != nullptr) {
            animation->play_attack_animation();
        }
        break;
    case CombatEffectType::RANGED_SHOOT:
        if (animation != nullptr && pending_projectile.active && projectile_config.has_value()) {
            animation->play_shoot_animation(false, projectile_config->spawn_animation_frame);
        } else if (animation != nullptr) {
            animation->play_shoot_animation();
            animation->consume_shoot_effect_triggered();
        }
        break;
    case CombatEffectType::DAMAGE_FLASH:
        if (AttackTarget *target = resolve_entity_id(command.target_id); target != nullptr) {
            target->flash_damage(to_godot_color(command.color));
        }
        break;
    case CombatEffectType::NONE:
        break;
    }
}

} // namespace

void CombatAttackExecutor::spawn_pending_projectile(const std::optional<ProjectileAttackConfig> &projectile_config, BattleEntity *unit,
                                                    AnimationController *animation, PendingProjectileSpawn &pending_projectile) {
    if (!pending_projectile.active) {
        return;
    }

    if (animation != nullptr && !animation->consume_shoot_effect_triggered()) {
        return;
    }

    Node *projectile_parent = unit != nullptr ? unit->get_parent() : nullptr;
    if (projectile_parent == nullptr) {
        projectile_parent = unit;
    }

    if (projectile_parent != nullptr && projectile_config.has_value()) {
        AttackTarget *direct_target = resolve_entity_id(pending_projectile.target_id);
        godot::Vector2 launch_position;
        if (animation != nullptr) {
            launch_position = animation->get_muzzle_global_position();
        } else if (unit != nullptr) {
            launch_position = unit->get_target_global_position();
        }

        ProjectileFactory::create(projectile_parent, *projectile_config, pending_projectile.shooter_side, pending_projectile.source_id,
                                  to_godot_color(pending_projectile.flash_color), launch_position, pending_projectile.target_global_position, direct_target,
                                  pending_projectile.fallback_damage);
    }

    pending_projectile = {};
}

void CombatAttackExecutor::apply_command(const CombatCommand &command, const std::optional<ProjectileAttackConfig> &projectile_config, BattleEntity *source,
                                         UnitSide shooter_side, AnimationController *animation, PendingProjectileSpawn &pending_projectile) {
    if (command.type == CombatCommandType::DEAL_DAMAGE) {
        if (AttackTarget *target = resolve_entity_id(command.target_id); target != nullptr) {
            const ObjectID source_id = source != nullptr ? ObjectID(source->get_instance_id()) : ObjectID();
            (void)DamageDispatcher::apply(source_id, target, command.damage);
        }
        return;
    }

    if (command.type == CombatCommandType::SPAWN_PROJECTILE) {
        pending_projectile = {
            .active = true,
            .target_id = command.target_id,
            .shooter_side = shooter_side,
            .source_id = source != nullptr ? ObjectID(source->get_instance_id()) : ObjectID(),
            .target_global_position = to_godot_vector(command.target_position),
            .fallback_damage = command.damage,
            .flash_color = command.color,
        };
        return;
    }

    if (command.type == CombatCommandType::PLAY_EFFECT) {
        apply_effect(command, projectile_config, animation, pending_projectile);
    }
}

} // namespace defn
