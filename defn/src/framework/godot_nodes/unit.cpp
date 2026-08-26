// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "unit.h"
#include "animation_controller.h"
#include "combat_component.h"
#include "field_promotion_view.h"
#include "health_component.h"
#include "hitbox_component.h"
#include "movement_component.h"
#include "unit_control_component.h"
#include "unit_factory.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

Unit::Unit() = default;

void Unit::_bind_methods() { ADD_SIGNAL(MethodInfo("unit_died", PropertyInfo(Variant::OBJECT, "unit"))); }

void Unit::set_unit_config(const UnitConfig &cfg) {
    unit_config_ = cfg;
    set_side(unit_config_.side);
    set_threat_weight(unit_config_.threat_weight);
    set_role(unit_config_.role);
    attack_range = unit_config_.melee_attack_range;
    ranged_range = unit_config_.ranged_attack_range;
}

void Unit::set_resolved_attack_ranges(real_t melee_range, real_t ranged_attack_range) {
    attack_range = melee_range;
    ranged_range = ranged_attack_range;
}

void Unit::configure_field_promotion(const FieldPromotionRules &rules) {
    field_promotion_.configure(rules, unit_config_.side == UnitSide::FRIENDLY && runtime_profile_.enable_combat);
}

void Unit::record_effective_damage_dealt(int effective_damage) {
    const FieldPromotionUpdate update = field_promotion_.record_effective_damage(effective_damage);
    if (!update.promotion_granted) {
        return;
    }
    if (combat != nullptr) {
        combat->apply_field_promotion(field_promotion_.get_rules());
    }
    if (health != nullptr) {
        health->set_max_hp_and_heal(apply_promoted_max_health(health->get_max_hp(), field_promotion_.get_rules()));
    }
    if (field_promotion_view != nullptr) {
        field_promotion_view->show_promotion();
    }
}

void Unit::_ready() {
    // Group assignment based on side
    if (unit_config_.side == UnitSide::FRIENDLY) {
        add_to_group("friendlies");
    } else {
        add_to_group("hostiles");
    }

    // Scale from config
    set_scale(godot::Vector2(unit_config_.scale, unit_config_.scale));

    UnitFactory::initialize(this);
}

void Unit::flash_damage(const godot::Color &color) {
    if (animation) {
        animation->flash_damage(color);
    }
}

bool Unit::is_commandable() const {
    return get_side() == UnitSide::FRIENDLY && !is_dead() && !is_queued_for_deletion() && runtime_profile_.enable_movement && movement != nullptr &&
           movement->get_speed_pixels_per_second() > 0.0F && unit_control_ != nullptr;
}

bool Unit::contains_selection_point(const godot::Vector2 &world_position, real_t fallback_radius) const {
    if (!is_commandable()) {
        return false;
    }

    const godot::Rect2 sprite_bounds = animation != nullptr ? animation->get_sprite_local_bounds() : godot::Rect2();
    if (sprite_bounds.has_area()) {
        return sprite_bounds.has_point(to_local(world_position));
    }

    const real_t safe_radius = Math::max(fallback_radius, 0.0F);
    return get_global_position().distance_squared_to(world_position) <= safe_radius * safe_radius;
}

real_t Unit::get_selection_ground_offset_y(real_t fallback_world_offset) const {
    const godot::Rect2 sprite_bounds = animation != nullptr ? animation->get_sprite_local_bounds() : godot::Rect2();
    if (!sprite_bounds.has_area()) {
        return fallback_world_offset;
    }

    return sprite_bounds.get_end().y * Math::abs(get_scale().y);
}

bool Unit::request_reposition(real_t destination_x, float arrival_epsilon) {
    return is_commandable() && unit_control_->request_reposition(destination_x, arrival_epsilon);
}

void Unit::cancel_reposition_for_match_end() {
    if (unit_control_ != nullptr) {
        unit_control_->cancel_without_combat_resume();
    }
}

void Unit::freeze_for_match_result(const StringName &animation_name) {
    if (is_dead() || is_queued_for_deletion()) {
        return;
    }

    cancel_reposition_for_match_end();

    if (movement != nullptr) {
        movement->stop();
    }
    if (combat != nullptr) {
        combat->set_enabled(false);
    }
    if (animation != nullptr) {
        animation->hide_muzzle_flash();
        animation->play_named_animation(animation_name);
    }
}

void Unit::on_died() {
    if (unit_control_ != nullptr) {
        unit_control_->cancel_without_combat_resume();
    }
    if (get_hitbox_component() != nullptr) {
        get_hitbox_component()->disable();
    }
    if (movement != nullptr) {
        movement->stop();
    }
    if (animation) {
        animation->set_anim_state(UnitPose::DEATH);
    }
    emit_signal("unit_died", this);
}

} // namespace defn
