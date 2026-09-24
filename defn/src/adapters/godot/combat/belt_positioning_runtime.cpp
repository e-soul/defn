// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "belt_positioning_runtime.h"

#include "animation_controller.h"
#include "combat_component.h"
#include "movement_component.h"
#include "unit.h"

#include <godot_cpp/core/object.hpp>

#include <set>
#include <vector>

namespace defn {

void BeltPositioningRuntime::clear() {
    solver_.clear();
    spawn_order_.clear();
    previous_x_.clear();
    next_order_ = 1;
}

void BeltPositioningRuntime::step(std::span<Unit *const> units, float top, float bottom, double delta) {
    std::vector<BeltUnitSnapshot> snapshots;
    std::map<uint64_t, Unit *> by_order;
    std::map<uint64_t, float> displacement_x;
    std::set<uint64_t> living_ids;
    for (Unit *unit : units) {
        if (unit == nullptr || unit->is_dead() || unit->is_queued_for_deletion()) {
            continue;
        }
        const uint64_t object_id = unit->get_instance_id();
        living_ids.insert(object_id);
        auto [order, inserted] = spawn_order_.try_emplace(object_id, next_order_);
        if (inserted) {
            ++next_order_;
        }
        auto *combat = godot::Object::cast_to<CombatComponent>(unit->get_node_or_null("CombatComponent"));
        auto *animation = godot::Object::cast_to<AnimationController>(unit->get_node_or_null("AnimationController"));
        const CombatTargetSelection selection = combat != nullptr ? combat->get_selection() : CombatTargetSelection{};
        const bool falling_back = combat != nullptr && combat->is_falling_back() && selection.has_unpassed_army;
        const godot::Vector2 position = unit->get_global_position();
        const float previous_x = previous_x_.contains(object_id) ? previous_x_[object_id] : static_cast<float>(position.x);
        by_order[order->second] = unit;
        displacement_x[order->second] = static_cast<float>(position.x) - previous_x;
        snapshots.push_back({
            .id = {.value = order->second},
            .side = unit->get_side(),
            .position = {.x = static_cast<float>(position.x), .y = static_cast<float>(position.y)},
            .approach_id = falling_back ? selection.unpassed_army_id : selection.approach_id,
            .approach_position = falling_back ? selection.unpassed_army_position : selection.approach_position,
            .attack_mode = selection.attack_mode,
            .manual = combat != nullptr && combat->is_manual_repositioning(),
            .moving = position.x != previous_x,
            .attacking = animation != nullptr && animation->is_attack_animation_playing(),
            .attack_y_speed_scale = animation != nullptr ? animation->get_animation_state().belt_y_speed_scale(unit->get_unit_config().belt_positioning) : 1.0F,
            .speed = unit->get_unit_config().belt_slide_speed_pixels_per_second,
            .config = unit->get_unit_config().belt_positioning,
        });
        previous_x_[object_id] = static_cast<float>(position.x);
    }
    std::erase_if(spawn_order_, [&living_ids](const auto &entry) { return !living_ids.contains(entry.first); });
    std::erase_if(previous_x_, [&living_ids](const auto &entry) { return !living_ids.contains(entry.first); });

    const std::vector<BeltPositionResult> result = solver_.advance(snapshots, top, bottom, delta);
    for (const BeltPositionResult &position_result : result) {
        Unit *unit = by_order.at(position_result.id.value);
        godot::Vector2 position = unit->get_global_position();
        const float displacement_y = position_result.next_y - static_cast<float>(position.y);
        position.y = position_result.next_y;
        unit->set_global_position(position);
        auto *animation = godot::Object::cast_to<AnimationController>(unit->get_node_or_null("AnimationController"));
        if (animation != nullptr) {
            animation->update_locomotion(displacement_x.at(position_result.id.value), displacement_y, delta, unit->get_unit_config().belt_positioning);
        }
    }
}

} // namespace defn
