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

float advance_belt_slide(float current_y, float target_y, float speed_pixels_per_second, double delta, float epsilon) {
    if (speed_pixels_per_second <= 0.0F || delta <= 0.0) {
        return current_y;
    }

    const float remaining = target_y - current_y;
    if (std::abs(remaining) <= std::max(epsilon, 0.0F)) {
        return target_y;
    }

    const float step = speed_pixels_per_second * static_cast<float>(delta);
    return current_y + std::copysign(std::min(step, std::abs(remaining)), remaining);
}

// A sensor is never allowed to be tighter than the gun. An `aggro_range` left at its default of zero therefore means
// "see exactly as far as you shoot", which is every unit shipped before pursuit existed and is why turning the
// mechanism on changes nothing until a catalog entry widens it.
float resolve_aggro_range(const CombatConfig &config) { return config.aggro_range > config.ranged_range ? config.aggro_range : config.ranged_range; }

// A unit with no attack of a kind carries -1 for that range, so the max is taken against zero as well: the standoff
// is a distance, never a sign.
float engagement_standoff(const CombatConfig &config) { return std::max({config.attack_range, config.ranged_range, 0.0F}); }

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
// A candidate's pull, as the shooter sees it: what the target broadcasts, times what this shooter thinks that kind of
// target is worth. The two compose rather than override, so a tank's aggro still drags a role-preferring shooter --
// just less far.
float effective_weight(const CombatConfig &config, const CombatTargetSnapshot &snapshot) {
    const float weight = snapshot.threat_weight > 0.0F ? snapshot.threat_weight : 1.0F;
    return weight * config.bias_for_role(snapshot.role);
}

float ranged_target_score(const CombatConfig &config, const CombatTargetSnapshot &snapshot, float distance) {
    const float weight = effective_weight(config, snapshot);
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
    // The nearest preferred-role candidate that is sensed but cannot be attacked from here yet. The reason to keep
    // walking, and -- for a melee-only pursuer, whose whole approach happens on this path -- the only lane there is
    // to slide toward.
    bool preferred_ahead = false;
    Vector2 preferred_ahead_position;
    // Whether the best thing that *can* be attacked is itself preferred, which is when there is nothing to wait for.
    bool best_is_preferred = false;
};

BestTargets scan_targets(const Vector2 &origin, const CombatConfig &config, std::span<const CombatTargetSnapshot> targets) {
    BestTargets best;
    float closest_melee_distance = std::numeric_limits<float>::max();
    float best_ranged_score = std::numeric_limits<float>::max();
    float closest_preferred_ahead_distance = std::numeric_limits<float>::max();
    bool best_is_preferred = false;

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

        if (!config.prefers_role(snapshot.role)) {
            continue;
        }

        // Preferred and reachable settles it; preferred and merely sensed is the reason not to settle for anything
        // else. The dead zone counts as unreachable on purpose -- walking into a target you cannot shoot yet is what
        // a minimum range is for.
        if (classify_target_by_distance(config, distance) != AttackMode::NONE) {
            best_is_preferred = true;
        } else if (distance <= resolve_aggro_range(config) && distance < closest_preferred_ahead_distance) {
            closest_preferred_ahead_distance = distance;
            best.preferred_ahead = true;
            best.preferred_ahead_position = snapshot.position;
        }
    }

    best.best_is_preferred = best_is_preferred;

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

// The first enemy this unit will walk into: nearest ahead, inside the sensor, whatever it is. Not a targeting rule --
// nothing chooses to attack by this -- only the answer to "what is this unit walking at", for a unit that has not
// picked anything yet because there is nothing it can reach.
struct NearestAhead {
    bool found = false;
    Vector2 position;
};

NearestAhead find_nearest_ahead(const Vector2 &origin, const CombatConfig &config, std::span<const CombatTargetSnapshot> targets) {
    NearestAhead nearest;
    float closest_distance = std::numeric_limits<float>::max();

    for (const CombatTargetSnapshot &snapshot : targets) {
        if (!snapshot.id.is_valid() || snapshot.dead || snapshot.side == config.side) {
            continue;
        }

        const float distance = get_forward_distance(config.side, origin, snapshot.position);
        if (distance < 0.0F || distance > resolve_aggro_range(config) || distance >= closest_distance) {
            continue;
        }

        closest_distance = distance;
        nearest.found = true;
        nearest.position = snapshot.position;
    }

    return nearest;
}

// Where the enemy army stands relative to this unit, ignoring structures entirely. A base is not part of the line a
// unit walks into, does not chase, and cannot be walked past -- and it is the thing a unit that has overrun the line
// would otherwise settle for, which is exactly the trade the fall-back rule exists to refuse.
struct ArmyLine {
    bool ahead = false;
    bool beyond_standoff = false;
    bool unpassed = false;
    Vector2 unpassed_position;
};

ArmyLine scan_army_line(const Vector2 &origin, const CombatConfig &config, std::span<const CombatTargetSnapshot> targets) {
    ArmyLine line;
    const float sensor = resolve_aggro_range(config);
    const float standoff = engagement_standoff(config);
    // The *least* overrun candidate: the one nearest to being properly in front, which for a unit walking backward is
    // the first it will reach. Signed, so a negative distance is behind and this maximum starts below every one of them.
    float nearest_unpassed_distance = std::numeric_limits<float>::lowest();

    for (const CombatTargetSnapshot &snapshot : targets) {
        if (!snapshot.id.is_valid() || snapshot.dead || snapshot.side == config.side || snapshot.role == UnitRole::STRUCTURE) {
            continue;
        }

        const float distance = get_forward_distance(config.side, origin, snapshot.position);
        // Behind counts, and counts at the same reach: the sensor is a circle on the field, and only the forward
        // distance's sign was ever making the half of it behind the unit invisible.
        if (std::abs(distance) > sensor) {
            continue;
        }

        line.ahead = line.ahead || distance >= 0.0F;
        line.beyond_standoff = line.beyond_standoff || distance >= standoff;

        if (distance < standoff && distance > nearest_unpassed_distance) {
            nearest_unpassed_distance = distance;
            line.unpassed = true;
            line.unpassed_position = snapshot.position;
        }
    }

    return line;
}

} // namespace

namespace {

// Who this unit attacks, if anything. The approach lane is decided separately, below.
CombatTargetSelection choose_target(const Vector2 &origin, const CombatConfig &config, EntityId current_target_id,
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

    // Pursuit. Something this shooter prefers is sensed ahead but cannot be hit from here, and nothing it *can* hit is
    // preferred -- so it declines to engage and the caller walks it forward, which is the only steering a lane needs.
    //
    // Deliberately after the melee-retention check above: a unit already in contact has stopped choosing, and pulling
    // it out of a fight it is in the middle of is a movement change rather than a targeting one. Nothing declares a
    // preferred role in the shipped catalog yet, so `preferred_ahead` is false everywhere and this never fires.
    if (best.preferred_ahead && !best.best_is_preferred) {
        return {.target_position = best.preferred_ahead_position, .pursuing = true};
    }

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

} // namespace

CombatTargetSelection select_target_from_snapshots(const Vector2 &origin, const CombatConfig &config, EntityId current_target_id,
                                                   std::span<const CombatTargetSnapshot> targets) {
    CombatTargetSelection selection = choose_target(origin, config, current_target_id, targets);

    // Only a unit that can decline an enemy it could already attack is able to end up behind one, so only such a unit
    // is asked where the line it walked through has got to.
    if (config.has_role_preference()) {
        const ArmyLine line = scan_army_line(origin, config, targets);
        selection.army_ahead = line.ahead;
        selection.army_beyond_standoff = line.beyond_standoff;
        selection.has_unpassed_army = line.unpassed;
        selection.unpassed_army_position = line.unpassed_position;
    }

    // Whatever it settled on is also what it is walking at.
    if (selection.target_id.is_valid() || selection.pursuing) {
        selection.has_approach_target = true;
        selection.approach_position = selection.target_position;
        return selection;
    }

    // Nothing in reach and nothing worth declining: the unit walks forward regardless, so the lane it steers for is
    // the first enemy it is going to arrive at. This is the ordinary case for a rusher for most of its run -- it can
    // see the line from hundreds of pixels out and cannot select any of it until it is in contact.
    const NearestAhead nearest = find_nearest_ahead(origin, config, targets);
    selection.has_approach_target = nearest.found;
    selection.approach_position = nearest.position;
    return selection;
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

// Whether the unit is walking back to the line it has overrun. The rule that starts it and the rule that ends it are
// deliberately different, which is why this is a mode and not a per-frame test.
//
// It starts when no part of the enemy army is in front any more -- the unit has run out of fight in the direction it
// was walking, and everything it could still kill is behind it. It ends only once some of the army is a full standoff
// in front, so the unit walks *past* its victim and turns around into the position an ordinary approach would have
// left it in. Ending it at the crossing instead would stop a melee rusher on top of the body it is about to swing at,
// because contact reach covers everything from zero.
//
// Neither rule counts structures, so a base standing between the unit and the map edge is not an excuse to keep
// walking: the army is always the better fight, and the base is not going anywhere.
bool decide_fall_back(const CombatLogicState &state, const CombatTargetSelection &selection) {
    if (!selection.has_unpassed_army) {
        return false;
    }

    return state.falling_back ? !selection.army_beyond_standoff : !selection.army_ahead;
}

// Reached only when nothing at all is in range, because target selection already re-engages anything that is. `walk`
// is which way: forward into the fight, or back toward the one this unit has left behind it.
void apply_disengaged_intents(const CombatLogicInput &input, CombatMovementIntent walk, CombatLogicStep &step) {
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
    step.intent.movement = walk;
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
        step.state.falling_back = false;
        return step;
    }

    const bool falling_back = decide_fall_back(input.state, input.selection);
    step.state.falling_back = falling_back;

    // Decided once, before any of the branches below, because the depth axis is independent of what happens on the
    // forward one: a unit that has stopped to swing should still finish nosing onto its target's lane, and a unit
    // frozen for a projectile's spawn frame is not thereby facing the wrong way. It reads the approach lane rather
    // than the selected target, so the curve is spread across the whole run-in.
    //
    // A unit falling back reads its lane off the enemy it is walking back to rather than off the selection, which by
    // then is either empty or pointing at whatever lies further forward. Same rule as the approach: steer at the thing
    // you are travelling toward, so the curve is spread over the whole run instead of snapping on arrival.
    if (falling_back) {
        step.intent.belt_slide = {.active = true, .target_y = input.selection.unpassed_army_position.y};
    } else if (input.selection.has_approach_target) {
        step.intent.belt_slide = {.active = true, .target_y = input.selection.approach_position.y};
    }

    // Falling back holds fire. The unit is on the wrong side of its victim, so anything it could select from here is
    // something it would rather attack from in front -- and a melee reach that starts at zero would otherwise let it
    // engage the instant it crossed, which is the whole thing the manoeuvre exists to avoid.
    const AttackMode attack_mode = falling_back ? AttackMode::NONE : input.selection.attack_mode;
    const bool mode_changed = attack_mode != input.state.attack_mode;
    step.state.engaged = input.selection.engaged && !falling_back;
    step.state.target_id = falling_back ? EntityId{} : input.selection.target_id;
    step.state.attack_mode = attack_mode;

    if (!input.attack_animation_playing && mode_changed && attack_mode != AttackMode::RANGED) {
        step.intent.hide_muzzle_flash = true;
    }

    if (input.projectile_pending) {
        step.intent.movement = CombatMovementIntent::STOP;
        return step;
    }

    if (step.state.engaged && step.state.target_id.is_valid()) {
        apply_engaged_intents(config, input, step);
        return step;
    }

    apply_disengaged_intents(input, falling_back ? CombatMovementIntent::FALL_BACK : CombatMovementIntent::MOVE, step);
    return step;
}

} // namespace defn
