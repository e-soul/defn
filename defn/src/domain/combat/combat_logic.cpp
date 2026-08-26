// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "combat_logic.h"

#include <algorithm>
#include <cmath>
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
    // Too far for contact and too close to shoot: the unit has to move before it can do anything, which is the whole
    // point of a minimum range.
    if (distance >= config.minimum_ranged_range && distance <= config.ranged_range) {
        return AttackMode::RANGED;
    }

    return AttackMode::NONE;
}

namespace {

// What a ranged shooter is reaching for, as one number: lower wins.
//
// Threat weight always makes a candidate more attractive, so it divides a score that is being minimised and
// multiplies one that has been negated to be minimised. A weight of exactly 1 leaves the score bit-identical to the
// raw distance the rule used before preferences existed, which is what keeps every default unit's behaviour unmoved.
//
// The range gate is deliberately not part of this. A unit still cannot shoot what it cannot reach; only the choice
// among the things it *can* reach is what a preference changes.
float ranged_target_score(const CombatConfig &config, const CombatTargetSnapshot &snapshot, float distance) {
    const float weight = snapshot.threat_weight > 0.0F ? snapshot.threat_weight : 1.0F;
    const auto health = static_cast<float>(snapshot.health);
    switch (config.target_preference) {
    case TargetPreference::FARTHEST:
        return -distance * weight;
    case TargetPreference::LOWEST_HP:
        return health / weight;
    case TargetPreference::HIGHEST_HP:
        return -health * weight;
    case TargetPreference::NEAREST:
        break;
    }

    return distance / weight;
}

// How much better a candidate has to be before a shooter abandons the target it is already firing at, as a fraction
// of the current target's score.
//
// Zero would re-pick every tick and make units thrash between candidates as the line walks. Infinity is what the rule
// used to be, and it is why aggro weight and target preference could only ever speak at the instant of first
// acquisition: a unit that had already locked on never asked the question again, and in a lane the first thing to
// cross the range gate is the nearest thing by construction.
constexpr float RANGED_RETARGET_MARGIN = 0.25F;

// The best candidate of each kind, chosen in one pass. Melee and ranged are picked by different rules, so they are
// tracked separately rather than one falling out of the other.
struct BestTargets {
    EntityId melee_id;
    Vector2 melee_position;
    EntityId ranged_id;
    Vector2 ranged_position;
    float ranged_score = std::numeric_limits<float>::max();
};

BestTargets scan_targets(const Vector2 &origin, const CombatConfig &config, std::span<const CombatTargetSnapshot> targets) {
    BestTargets best;
    float closest_melee_distance = std::numeric_limits<float>::max();
    float best_ranged_score = std::numeric_limits<float>::max();

    for (const CombatTargetSnapshot &snapshot : targets) {
        if (!snapshot.id.is_valid() || snapshot.dead || snapshot.side == config.side) {
            continue;
        }

        const float distance = get_forward_distance(config.side, origin, snapshot.position);
        if (distance < 0.0F) {
            continue;
        }

        // Melee stays on pure distance. Contact is decided by who you are standing next to, and a preference that
        // reached past the unit in your face would be a movement change wearing a targeting change's clothes.
        if (distance <= config.attack_range && distance < closest_melee_distance) {
            closest_melee_distance = distance;
            best.melee_id = snapshot.id;
            best.melee_position = snapshot.position;
        }

        const bool shootable = distance >= config.minimum_ranged_range && distance <= config.ranged_range;
        const float score = shootable ? ranged_target_score(config, snapshot, distance) : std::numeric_limits<float>::max();
        if (score < best_ranged_score) {
            best_ranged_score = score;
            best.ranged_id = snapshot.id;
            best.ranged_position = snapshot.position;
        }
    }

    best.ranged_score = best_ranged_score;
    return best;
}

// What the unit is already fighting, if it is still there and still reachable.
struct RetainedTarget {
    AttackMode mode = AttackMode::NONE;
    Vector2 position;
    float ranged_score = std::numeric_limits<float>::max();
};

RetainedTarget find_retained_target(const Vector2 &origin, const CombatConfig &config, EntityId current_target_id,
                                    std::span<const CombatTargetSnapshot> targets) {
    if (!current_target_id.is_valid()) {
        return {};
    }

    for (const CombatTargetSnapshot &snapshot : targets) {
        if (snapshot.id != current_target_id || snapshot.dead || snapshot.side == config.side) {
            continue;
        }

        const float distance = get_forward_distance(config.side, origin, snapshot.position);
        return {
            .mode = classify_target_by_distance(config, distance),
            .position = snapshot.position,
            .ranged_score = ranged_target_score(config, snapshot, distance),
        };
    }

    return {};
}

// Sign-safe on purpose: `farthest` and `highest_hp` score negative, so the margin is taken against the magnitude of
// what the unit is currently shooting rather than multiplied through it.
bool clears_retarget_margin(float candidate_score, float retained_score) {
    return candidate_score < retained_score - (RANGED_RETARGET_MARGIN * std::abs(retained_score));
}

} // namespace

CombatTargetSelection select_target_from_snapshots(const Vector2 &origin, const CombatConfig &config, EntityId current_target_id,
                                                   std::span<const CombatTargetSnapshot> targets) {
    const RetainedTarget retained = find_retained_target(origin, config, current_target_id, targets);

    // Contact is sticky. Who you are standing next to is not a choice a preference gets to revisit, and letting one
    // walk a unit away mid-swing would be a movement change wearing a targeting change's clothes.
    if (retained.mode == AttackMode::MELEE) {
        return {
            .engaged = true,
            .attack_mode = AttackMode::MELEE,
            .target_id = current_target_id,
            .target_position = retained.position,
        };
    }

    const BestTargets best = scan_targets(origin, config, targets);

    // Ranged fire re-asks the question, but only answers differently when the answer is clearly better. Below the
    // margin the unit keeps firing at what it already had.
    if (retained.mode == AttackMode::RANGED && !clears_retarget_margin(best.ranged_score, retained.ranged_score)) {
        return {
            .engaged = true,
            .attack_mode = AttackMode::RANGED,
            .target_id = current_target_id,
            .target_position = retained.position,
        };
    }

    if (best.melee_id.is_valid()) {
        return {
            .engaged = true,
            .attack_mode = AttackMode::MELEE,
            .target_id = best.melee_id,
            .target_position = best.melee_position,
        };
    }

    if (best.ranged_id.is_valid()) {
        return {
            .engaged = true,
            .attack_mode = AttackMode::RANGED,
            .target_id = best.ranged_id,
            .target_position = best.ranged_position,
        };
    }

    return {};
}

namespace {

void apply_engaged_intents(const CombatConfig &config, const CombatLogicInput &input, CombatLogicStep &step) {
    step.intent.movement = CombatMovementIntent::STOP;

    // A running attack animation owns the sprite; re-posing here would freeze it mid-swing.
    if (!input.attack_animation_playing) {
        if (input.selection.attack_mode == AttackMode::MELEE && input.current_pose != CombatPoseState::ATTACK) {
            step.intent.pose = CombatPoseIntent::ATTACK;
        } else if (input.selection.attack_mode == AttackMode::RANGED && input.current_pose != CombatPoseState::SHOOT) {
            step.intent.pose = CombatPoseIntent::SHOOT;
        }
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
}

// Reached only when nothing at all is in range, because target selection already re-engages anything that is.
void apply_disengaged_intents(const CombatLogicInput &input, CombatLogicStep &step) {
    step.state.engaged = false;
    step.state.target_id = {};
    step.state.attack_mode = AttackMode::NONE;

    // The windup always plays. Past it, the backswing gives way to a chase when the target merely fled; when it died
    // there is nothing to chase, so the animation is left to finish rather than sliding the unit forward.
    const bool backswing_cancelable = !input.attack_windup_active && input.target_out_of_range;
    if (input.attack_animation_playing && !backswing_cancelable) {
        step.intent.movement = CombatMovementIntent::STOP;
        return;
    }

    step.intent.hide_muzzle_flash = true;
    step.intent.movement = CombatMovementIntent::MOVE;
    if (input.current_pose == CombatPoseState::ATTACK || input.current_pose == CombatPoseState::SHOOT) {
        step.intent.pose = CombatPoseIntent::WALK;
    }
}

} // namespace

CombatLogicStep advance_combat_logic(const CombatConfig &config, const CombatLogicInput &input) {
    CombatLogicStep step;
    step.state = input.state;

    if (input.unit_dead) {
        return step;
    }

    step.state.attack_cooldown_seconds = std::max(step.state.attack_cooldown_seconds - input.delta, 0.0);

    if (input.manual_repositioning) {
        step.state.attack_mode = AttackMode::NONE;
        step.state.engaged = false;
        step.state.target_id = {};
        return step;
    }

    const bool mode_changed = input.selection.attack_mode != input.state.attack_mode;
    step.state.engaged = input.selection.engaged;
    step.state.target_id = input.selection.target_id;
    step.state.attack_mode = input.selection.attack_mode;

    if (!input.attack_animation_playing && mode_changed && input.selection.attack_mode != AttackMode::RANGED) {
        step.intent.hide_muzzle_flash = true;
    }

    if (input.projectile_pending) {
        step.intent.movement = CombatMovementIntent::STOP;
        return step;
    }

    if (input.selection.engaged && input.selection.target_id.is_valid()) {
        apply_engaged_intents(config, input, step);
        return step;
    }

    apply_disengaged_intents(input, step);
    return step;
}

} // namespace defn
