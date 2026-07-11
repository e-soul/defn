#include "unit.h"
#include "animation_controller.h"
#include "combat_component.h"
#include "field_promotion_view.h"
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
