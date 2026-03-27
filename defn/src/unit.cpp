#include "unit.h"
#include "animation_controller.h"
#include "combat_component.h"
#include "detection_component.h"
#include "grid_manager.h"
#include "health_bar_widget.h"
#include "health_component.h"
#include "sound_controller.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

Unit::Unit() {
    // ±20% random variation on base attack range to prevent perfect alignment
    double variation = UtilityFunctions::randf_range(0.8, 1.2);
    attack_range = GridManager::ATTACK_RANGE * variation;
    double ranged_variation = UtilityFunctions::randf_range(0.8, 1.2);
    ranged_range = GridManager::RANGED_RANGE * ranged_variation;
}

void Unit::_bind_methods() {
    ADD_SIGNAL(MethodInfo("unit_died", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("enemy_breached"));
}

void Unit::set_unit_config(const UnitConfig &cfg) { unit_config_ = cfg; }

void Unit::_ready() {
    // Group assignment based on side
    if (unit_config_.side == UnitSide::FRIENDLY) {
        add_to_group("friendlies");
    } else {
        add_to_group("hostiles");
    }

    // Scale from config
    set_scale(Vector2(static_cast<real_t>(unit_config_.scale), static_cast<real_t>(unit_config_.scale)));

    // Health
    health = memnew(HealthComponent);
    health->set_name("HealthComponent");
    add_child(health);
    health->configure(unit_config_.hp);
    health->connect("died", callable_mp(this, &Unit::on_died));

    // Health bar
    health_bar_widget = memnew(HealthBarWidget);
    health_bar_widget->set_name("HealthBarWidget");
    add_child(health_bar_widget);
    health_bar_widget->configure(health, unit_config_.health_bar_color, unit_config_.health_bar_offset);

    // Animation
    animation = memnew(AnimationController);
    animation->set_name("AnimationController");
    add_child(animation);
    animation->configure(this, unit_config_);

    sound = memnew(SoundController);
    sound->set_name("SoundController");
    add_child(sound);
    sound->configure(this, unit_config_);
    animation->connect("shoot_effect_triggered", callable_mp(sound, &SoundController::play_shoot_sfx));

    // Detection
    detection = memnew(DetectionComponent);
    detection->set_name("DetectionComponent");
    add_child(detection);
    double scale_x = get_scale().x;
    if (unit_config_.side == UnitSide::FRIENDLY) {
        detection->configure(this, 1, 2, ranged_range, scale_x);
    } else {
        detection->configure(this, 2, 1, ranged_range, scale_x);
    }

    // Combat
    combat = memnew(CombatComponent);
    combat->set_name("CombatComponent");
    add_child(combat);
    CombatComponent::Config combat_cfg{
        .side = unit_config_.side,
        .melee_damage = unit_config_.melee_damage,
        .melee_attack_period_seconds = unit_config_.melee_attack_period_seconds,
        .ranged_damage = unit_config_.ranged_damage,
        .ranged_attack_period_seconds = unit_config_.ranged_attack_period_seconds,
        .attack_range = attack_range,
        .ranged_range = ranged_range,
        .melee_flash_color = unit_config_.melee_flash_color,
        .ranged_flash_color = unit_config_.ranged_flash_color,
    };
    combat->configure(this, health, animation, detection->get_detection_area(), combat_cfg);
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
    double speed = unit_config_.move_speed * GridManager::ATTACK_RANGE;

    if (unit_config_.side == UnitSide::FRIENDLY) {
        double max_x = grid->get_world_width() - 100.0;
        if (get_position().x < max_x) {
            set_velocity(Vector2(static_cast<real_t>(speed), 0));
            move_and_slide();
            if (get_position().x > max_x) {
                set_position(Vector2(static_cast<real_t>(max_x), get_position().y));
                set_velocity(Vector2(0, 0));
            }
        } else {
            set_velocity(Vector2(0, 0));
        }
    } else {
        set_velocity(Vector2(static_cast<real_t>(-speed), 0));
        move_and_slide();
    }
}

void Unit::notify_breach() {
    emit_signal("enemy_breached");
    queue_free();
}

} // namespace defn
