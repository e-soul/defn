// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "combat_target_selector.h"

#include "attack_target_resolver.h"
#include "battle_entity.h"
#include "godot_vector.h"

#include <algorithm>
#include <vector>

namespace defn {

namespace {

EntityId entity_id_for(const AttackTarget &target) { return {.value = static_cast<uint64_t>(target.get_target_object_id())}; }

AttackTarget *resolve_entity_id(EntityId entity_id) { return entity_id.is_valid() ? resolve_attack_target(ObjectID(entity_id.value)) : nullptr; }

} // namespace

CombatTargetSelection CombatTargetSelector::select(const BattleEntity *unit, Area2D *detection_area, const CombatConfig &config, EntityId current_target_id) {
    if (unit == nullptr || detection_area == nullptr) {
        return {};
    }

    const Vector2 origin = to_vector(unit->get_target_global_position());
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
            .id = entity_id_for(*target),
            .side = target->get_side(),
            .dead = target->is_dead(),
            .position = to_vector(target->get_target_global_position()),
        });
    }

    AttackTarget *current_target = resolve_entity_id(current_target_id);
    if (current_target != nullptr) {
        const bool has_current_target =
            std::ranges::any_of(snapshots, [current_target_id](const CombatTargetSnapshot &snapshot) { return snapshot.id == current_target_id; });
        if (!has_current_target) {
            snapshots.push_back({
                .id = current_target_id,
                .side = current_target->get_side(),
                .dead = current_target->is_dead(),
                .position = to_vector(current_target->get_target_global_position()),
            });
        }
    }

    return select_target_from_snapshots(origin, config, current_target_id, snapshots);
}

bool CombatTargetSelector::is_target_out_of_range(const BattleEntity *unit, const CombatConfig &config, EntityId target_id) {
    if (unit == nullptr) {
        return false;
    }

    AttackTarget *target = resolve_entity_id(target_id);
    if (target == nullptr || target->is_dead() || target->get_side() == config.side) {
        return false;
    }

    const float distance = get_forward_distance(config.side, to_vector(unit->get_target_global_position()), to_vector(target->get_target_global_position()));
    return classify_target_by_distance(config, distance) == AttackMode::NONE;
}

} // namespace defn