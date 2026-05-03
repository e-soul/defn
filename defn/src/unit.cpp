#include "unit.h"
#include "animation_controller.h"
#include "health_component.h"
#include "movement_component.h"
#include "unit_factory.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

Unit::Unit() = default;

void Unit::_bind_methods() { ADD_SIGNAL(MethodInfo("unit_died", PropertyInfo(Variant::OBJECT, "unit"))); }

void Unit::set_unit_config(const UnitConfig &cfg) {
    unit_config_ = cfg;
    set_side(unit_config_.side);

    const auto melee_variation =
        static_cast<real_t>(UtilityFunctions::randf_range(unit_config_.melee_attack_range_variation.min, unit_config_.melee_attack_range_variation.max));
    attack_range = unit_config_.melee_attack_range * melee_variation;

    const auto ranged_variation =
        static_cast<real_t>(UtilityFunctions::randf_range(unit_config_.ranged_attack_range_variation.min, unit_config_.ranged_attack_range_variation.max));
    ranged_range = unit_config_.ranged_attack_range * ranged_variation;
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

void Unit::flash_damage(const Color &color) {
    if (animation) {
        animation->flash_damage(color);
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
