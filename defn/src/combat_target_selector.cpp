#include "combat_target_selector.h"

#include "attack_target_resolver.h"
#include "unit.h"

#include <algorithm>
#include <vector>

namespace defn {

CombatTargetSelection CombatTargetSelector::select(Unit *unit, Area2D *detection_area, const CombatConfig &config, AttackTarget *current_target) {
    if (unit == nullptr || detection_area == nullptr) {
        return {};
    }

    const Vector2 origin = unit->get_global_position();
    std::vector<CombatTargetSnapshot> snapshots;
    const TypedArray<Area2D> overlapping = detection_area->get_overlapping_areas();
    snapshots.reserve(overlapping.size());
    for (const auto &area_variant : overlapping) {
        auto *hitbox = Object::cast_to<Area2D>(area_variant.operator Object *());
        if (hitbox == nullptr) {
            continue;
        }

        auto *target = resolve_attack_target(hitbox->get_parent());
        if (target == nullptr) {
            continue;
        }

        snapshots.push_back({
            .target = target,
            .side = target->get_side(),
            .dead = target->is_dead(),
            .position = target->get_target_global_position(),
        });
    }

    if (current_target != nullptr) {
        const bool has_current_target =
            std::ranges::any_of(snapshots, [current_target](const CombatTargetSnapshot &snapshot) { return snapshot.target == current_target; });
        if (!has_current_target) {
            snapshots.push_back({
                .target = current_target,
                .side = current_target->get_side(),
                .dead = current_target->is_dead(),
                .position = current_target->get_target_global_position(),
            });
        }
    }

    return select_target_from_snapshots(origin, config, current_target, snapshots);
}

} // namespace defn