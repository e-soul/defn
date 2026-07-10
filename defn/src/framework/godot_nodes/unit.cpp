#include "unit.h"
#include "animation_controller.h"
#include "combat_component.h"
#include "health_component.h"
#include "movement_component.h"
#include "unit_factory.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

Unit::Unit() = default;

void Unit::_bind_methods() { ADD_SIGNAL(MethodInfo("unit_died", PropertyInfo(Variant::OBJECT, "unit"))); }

void Unit::set_unit_config(const UnitConfig &cfg) {
    unit_config_ = cfg;
    set_side(unit_config_.side);
    attack_range = unit_config_.melee_attack_range;
    ranged_range = unit_config_.ranged_attack_range;
}

void Unit::set_resolved_attack_ranges(real_t melee_range, real_t ranged_attack_range) {
    attack_range = melee_range;
    ranged_range = ranged_attack_range;
}

void Unit::_ready() {
    // Group assignment based on side
    if (unit_config_.side == UnitSide::FRIENDLY) {
        add_to_group("friendlies");
    } else {
        add_to_group("hostiles");
    }

    // Scale from config
    set_scale(Vector2(unit_config_.scale, unit_config_.scale));

    UnitFactory::initialize(this);
}

void Unit::flash_damage(const godot::Color &color) {
    if (animation) {
        animation->flash_damage(color);
    }
}

void Unit::freeze_for_match_result(const StringName &animation_name) {
    if (is_dead() || is_queued_for_deletion()) {
        return;
    }

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
    if (movement != nullptr) {
        movement->stop();
    }
    if (animation) {
        animation->set_anim_state(AnimState::DEATH);
    }
    emit_signal("unit_died", this);
}

} // namespace defn
