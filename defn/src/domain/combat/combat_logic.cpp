#include "combat_logic.h"

#include <algorithm>
#include <limits>

namespace defn {

float get_forward_distance(UnitSide side, const Vector2 &origin, const Vector2 &target_position) {
    if (side == UnitSide::FRIENDLY) {
        return target_position.x - origin.x;
    }

    return origin.x - target_position.x;
}

AttackMode classify_target_by_distance(const CombatConfig &config, float distance) {
    if (distance < 0.0F) {
        return AttackMode::NONE;
    }
    if (distance <= config.attack_range) {
        return AttackMode::MELEE;
    }
    if (distance <= config.ranged_range) {
        return AttackMode::RANGED;
    }

    return AttackMode::NONE;
}

CombatTargetSelection select_target_from_snapshots(const Vector2 &origin, const CombatConfig &config, EntityId current_target_id,
                                                   std::span<const CombatTargetSnapshot> targets) {
    if (current_target_id.is_valid()) {
        for (const CombatTargetSnapshot &snapshot : targets) {
            if (snapshot.id != current_target_id || snapshot.dead || snapshot.side == config.side) {
                continue;
            }

            const AttackMode current_mode = classify_target_by_distance(config, get_forward_distance(config.side, origin, snapshot.position));
            if (current_mode != AttackMode::NONE) {
                return {
                    .engaged = true,
                    .attack_mode = current_mode,
                    .target_id = current_target_id,
                    .target_position = snapshot.position,
                };
            }
            break;
        }
    }

    EntityId best_melee_target_id;
    EntityId best_ranged_target_id;
    Vector2 best_melee_target_position;
    Vector2 best_ranged_target_position;
    float closest_melee_distance = std::numeric_limits<float>::max();
    float closest_ranged_distance = std::numeric_limits<float>::max();

    for (const CombatTargetSnapshot &snapshot : targets) {
        if (!snapshot.id.is_valid() || snapshot.dead || snapshot.side == config.side) {
            continue;
        }

        const float distance = get_forward_distance(config.side, origin, snapshot.position);
        if (distance < 0.0F) {
            continue;
        }

        if (distance <= config.attack_range && distance < closest_melee_distance) {
            closest_melee_distance = distance;
            best_melee_target_id = snapshot.id;
            best_melee_target_position = snapshot.position;
        }
        if (distance <= config.ranged_range && distance < closest_ranged_distance) {
            closest_ranged_distance = distance;
            best_ranged_target_id = snapshot.id;
            best_ranged_target_position = snapshot.position;
        }
    }

    if (best_melee_target_id.is_valid()) {
        return {
            .engaged = true,
            .attack_mode = AttackMode::MELEE,
            .target_id = best_melee_target_id,
            .target_position = best_melee_target_position,
        };
    }

    if (best_ranged_target_id.is_valid()) {
        return {
            .engaged = true,
            .attack_mode = AttackMode::RANGED,
            .target_id = best_ranged_target_id,
            .target_position = best_ranged_target_position,
        };
    }

    return {};
}

CombatLogicStep advance_combat_logic(const CombatConfig &config, const CombatLogicInput &input) {
    CombatLogicStep step;
    step.state = input.state;

    if (input.unit_dead) {
        return step;
    }

    step.state.attack_cooldown_seconds = std::max(step.state.attack_cooldown_seconds - input.delta, 0.0);

    const bool mode_changed = input.selection.attack_mode != input.state.attack_mode;
    step.state.engaged = input.selection.engaged;
    step.state.target_id = input.selection.target_id;
    step.state.attack_mode = input.selection.attack_mode;

    if (mode_changed && input.selection.attack_mode != AttackMode::RANGED) {
        step.intent.hide_muzzle_flash = true;
    }

    if (input.projectile_pending) {
        step.intent.movement = CombatMovementIntent::STOP;
        return step;
    }

    if (input.selection.engaged && input.selection.target_id.is_valid()) {
        step.intent.movement = CombatMovementIntent::STOP;

        if (input.selection.attack_mode == AttackMode::MELEE && input.current_pose != CombatPoseState::ATTACK) {
            step.intent.pose = CombatPoseIntent::ATTACK;
        } else if (input.selection.attack_mode == AttackMode::RANGED && input.current_pose != CombatPoseState::SHOOT) {
            step.intent.pose = CombatPoseIntent::SHOOT;
        }

        double attack_period_seconds = 0.0;
        if (input.selection.attack_mode == AttackMode::MELEE) {
            attack_period_seconds = config.melee_attack_period_seconds;
        } else if (input.selection.attack_mode == AttackMode::RANGED) {
            attack_period_seconds = config.ranged_attack_period_seconds;
        }

        if (attack_period_seconds > 0.0 && step.state.attack_cooldown_seconds <= 0.0) {
            step.intent.trigger_attack = true;
            step.state.attack_cooldown_seconds = attack_period_seconds;
        }

        return step;
    }

    step.state.engaged = false;
    step.state.target_id = {};
    step.state.attack_mode = AttackMode::NONE;
    step.state.attack_cooldown_seconds = 0.0;
    step.intent.hide_muzzle_flash = true;
    step.intent.movement = CombatMovementIntent::MOVE;
    if (input.current_pose == CombatPoseState::ATTACK || input.current_pose == CombatPoseState::SHOOT) {
        step.intent.pose = CombatPoseIntent::WALK;
    }

    return step;
}

} // namespace defn