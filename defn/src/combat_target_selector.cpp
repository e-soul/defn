#include "combat_target_selector.h"

#include "attack_target_resolver.h"
#include "unit.h"

#include <limits>

namespace defn {

CombatTargetSelection CombatTargetSelector::select(Unit *unit, Area2D *detection_area, const CombatConfig &config, AttackTarget *current_target) {
    if (unit == nullptr || detection_area == nullptr) {
        return {};
    }

    const AttackMode current_mode = classify_target(unit, config, current_target);
    if (current_mode != AttackMode::NONE) {
        return {
            .engaged = true,
            .attack_mode = current_mode,
            .target = current_target,
        };
    }

    AttackTarget *best_melee_target = nullptr;
    AttackTarget *best_ranged_target = nullptr;
    real_t closest_melee_distance = std::numeric_limits<real_t>::max();
    real_t closest_ranged_distance = std::numeric_limits<real_t>::max();

    const Vector2 origin = unit->get_global_position();
    const TypedArray<Area2D> overlapping = detection_area->get_overlapping_areas();
    for (const auto &area_variant : overlapping) {
        auto *hitbox = Object::cast_to<Area2D>(area_variant.operator Object *());
        if (hitbox == nullptr) {
            continue;
        }

        auto *target = resolve_attack_target(hitbox->get_parent());
        if (target == nullptr || target->is_dead() || target->get_side() == config.side) {
            continue;
        }

        const real_t distance = get_forward_distance(config.side, origin, target);
        if (distance < 0) {
            continue;
        }

        if (distance <= config.attack_range && distance < closest_melee_distance) {
            closest_melee_distance = distance;
            best_melee_target = target;
        }
        if (distance <= config.ranged_range && distance < closest_ranged_distance) {
            closest_ranged_distance = distance;
            best_ranged_target = target;
        }
    }

    if (best_melee_target != nullptr) {
        return {
            .engaged = true,
            .attack_mode = AttackMode::MELEE,
            .target = best_melee_target,
        };
    }

    if (best_ranged_target != nullptr) {
        return {
            .engaged = true,
            .attack_mode = AttackMode::RANGED,
            .target = best_ranged_target,
        };
    }

    return {};
}

AttackMode CombatTargetSelector::classify_target(Unit *unit, const CombatConfig &config, const AttackTarget *target) {
    if (unit == nullptr || target == nullptr || target->is_dead()) {
        return AttackMode::NONE;
    }

    const real_t distance = get_forward_distance(config.side, unit->get_global_position(), target);
    if (distance < 0) {
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

real_t CombatTargetSelector::get_forward_distance(UnitSide side, const Vector2 &origin, const AttackTarget *target) {
    if (target == nullptr) {
        return -1.0F;
    }

    if (side == UnitSide::FRIENDLY) {
        return target->get_target_global_position().x - origin.x;
    }

    return origin.x - target->get_target_global_position().x;
}

} // namespace defn