#include "unit.h"
#include "animation_controller.h"
#include "grid_manager.h"
#include "health_component.h"
#include "unit_factory.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

Unit::Unit() = default;

void Unit::_bind_methods() {
    ADD_SIGNAL(MethodInfo("unit_died", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("enemy_breached"));
}

void Unit::set_unit_config(const UnitConfig &cfg) {
    unit_config_ = cfg;

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

void Unit::take_damage(int amount) {
    if (health) {
        health->take_damage(amount);
    }
}

void Unit::flash_damage(const Color &color) {
    if (animation) {
        animation->flash_damage(color);
    }
}

bool Unit::is_dead() const { return health ? health->is_dead() : true; }

void Unit::on_died() {
    set_velocity(Vector2(0, 0));
    if (animation) {
        animation->set_anim_state(AnimState::DEATH);
    }
    emit_signal("unit_died", this);
}

void Unit::do_movement(double delta) {
    auto *grid = GridManager::get_singleton();
    if (grid == nullptr) {
        return;
    }

    const real_t speed = unit_config_.move_speed_pixels_per_second;
    const auto &rules = grid->get_rules();

    if (unit_config_.side == UnitSide::FRIENDLY) {
        const real_t max_x = grid->get_world_width() - rules.friendly_world_margin;
        if (get_position().x < max_x) {
            set_velocity(Vector2(speed, 0));
            move_and_slide();
            if (get_position().x > max_x) {
                set_position(Vector2(max_x, get_position().y));
                set_velocity(Vector2(0, 0));
            }
        } else {
            set_velocity(Vector2(0, 0));
        }
    } else {
        set_velocity(Vector2(-speed, 0));
        move_and_slide();
    }
}

void Unit::notify_breach() {
    emit_signal("enemy_breached");
    queue_free();
}

} // namespace defn
