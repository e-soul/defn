#include "unit_factory.h"

#include "animation_controller.h"
#include "collision_layers.h"
#include "combat_component.h"
#include "detection_component.h"
#include "health_bar_widget.h"
#include "health_component.h"
#include "hitbox_component.h"
#include "movement_component.h"
#include "sound_controller.h"
#include "unit.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

namespace {

constexpr real_t DEFAULT_HITBOX_RADIUS = 5.0F;

HealthComponent *create_health_component(Unit *unit) {
    auto *health = memnew(HealthComponent);
    health->set_name("HealthComponent");
    unit->add_child(health);
    health->configure(unit->get_unit_config().hp);
    return health;
}

HealthBarWidget *create_health_bar_widget(Unit *unit, HealthComponent *health) {
    auto *health_bar_widget = memnew(HealthBarWidget);
    health_bar_widget->set_name("HealthBarWidget");
    unit->add_child(health_bar_widget);
    health_bar_widget->configure(health, unit->get_unit_config().health_bar_color, unit->get_unit_config().health_bar_offset);
    return health_bar_widget;
}

AnimationController *create_animation_controller(Unit *unit) {
    auto *animation = memnew(AnimationController);
    animation->set_name("AnimationController");
    unit->add_child(animation);
    animation->configure(unit, unit->get_unit_config());
    return animation;
}

SoundController *create_sound_controller(Unit *unit, AnimationController *animation) {
    auto *sound = memnew(SoundController);
    sound->set_name("SoundController");
    unit->add_child(sound);
    sound->configure(unit, unit->get_unit_config());
    animation->connect("shoot_effect_triggered", callable_mp(sound, &SoundController::play_shoot_sfx));
    return sound;
}

HitboxComponent *create_hitbox_component(Unit *unit) {
    auto *hitbox = memnew(HitboxComponent);
    hitbox->set_name("HitboxComponent");
    unit->add_child(hitbox);

    const real_t scale_x = unit->get_scale().x;
    const auto detection_channels = get_detection_channels(unit->get_side());
    hitbox->configure(unit, detection_channels.hitbox_layer, DEFAULT_HITBOX_RADIUS / scale_x);
    return hitbox;
}

DetectionComponent *create_detection_component(Unit *unit) {
    auto *detection = memnew(DetectionComponent);
    detection->set_name("DetectionComponent");
    unit->add_child(detection);

    const real_t scale_x = unit->get_scale().x;
    const auto detection_channels = get_detection_channels(unit->get_side());
    detection->configure(unit, detection_channels.sensor_mask, unit->get_ranged_range(), scale_x);
    return detection;
}

MovementComponent *create_movement_component(Unit *unit) {
    auto *movement = memnew(MovementComponent);
    movement->set_name("MovementComponent");
    unit->add_child(movement);
    movement->configure(unit, unit->get_side(), unit->get_unit_config().move_speed_pixels_per_second);
    return movement;
}

CombatComponent::Config make_combat_config(const Unit *unit) {
    const auto &config = unit->get_unit_config();
    CombatComponent::Config combat_config;
    const bool has_melee_attack = config.melee_damage > 0;
    const bool has_ranged_attack = config.ranged_damage > 0 || config.projectile_attack.has_value();
    combat_config.side = config.side;
    combat_config.melee_damage = config.melee_damage;
    combat_config.melee_attack_period_seconds = config.melee_attack_period_seconds;
    combat_config.ranged_damage = config.ranged_damage;
    combat_config.ranged_attack_period_seconds = config.ranged_attack_period_seconds;
    combat_config.attack_range = has_melee_attack ? unit->get_attack_range() : -1.0F;
    combat_config.ranged_range = has_ranged_attack ? unit->get_ranged_range() : -1.0F;
    combat_config.melee_flash_color = config.melee_flash_color;
    combat_config.ranged_flash_color = config.ranged_flash_color;
    combat_config.projectile_attack = config.projectile_attack;
    return combat_config;
}

CombatComponent *create_combat_component(Unit *unit, HealthComponent *health, AnimationController *animation, Area2D *detection_area) {
    auto *combat = memnew(CombatComponent);
    combat->set_name("CombatComponent");
    unit->add_child(combat);
    combat->configure(unit, health, animation, detection_area, make_combat_config(unit));
    return combat;
}

} // namespace

Unit *UnitFactory::create(const UnitConfig &config, const Vector2 &position) { return create(config, position, UnitRuntimeProfile::from_unit_config(config)); }

Unit *UnitFactory::create(const UnitConfig &config, const Vector2 &position, const UnitRuntimeProfile &profile) {
    auto *unit = memnew(Unit);
    unit->set_unit_config(config);
    unit->set_runtime_profile(profile);
    unit->set_position(position);
    return unit;
}

Unit *UnitFactory::materialize(const UnitSpawnRequest &request) { return materialize(request, UnitRuntimeProfile::from_unit_config(request.config)); }

Unit *UnitFactory::materialize(const UnitSpawnRequest &request, const UnitRuntimeProfile &profile) { return create(request.config, request.position, profile); }

void UnitFactory::initialize(Unit *unit) {
    if (unit == nullptr || unit->runtime_initialized_) {
        return;
    }

    const UnitRuntimeProfile &profile = unit->get_runtime_profile();
    const bool enable_sensor = profile.enable_target_sensor || profile.enable_combat;

    unit->attach_hitbox_component(create_hitbox_component(unit));
    unit->health = create_health_component(unit);
    unit->attach_health_component(unit->health);
    unit->health->connect("died", callable_mp(unit, &Unit::on_died));
    if (profile.enable_health_bar) {
        unit->health_bar_widget = create_health_bar_widget(unit, unit->health);
    }
    unit->animation = create_animation_controller(unit);
    if (profile.enable_sound) {
        unit->sound = create_sound_controller(unit, unit->animation);
    }
    if (enable_sensor) {
        unit->detection = create_detection_component(unit);
    }
    if (profile.enable_movement) {
        unit->movement = create_movement_component(unit);
    }
    if (profile.enable_combat) {
        Area2D *detection_area = unit->detection != nullptr ? unit->detection->get_detection_area() : nullptr;
        unit->combat = create_combat_component(unit, unit->health, unit->animation, detection_area);
    }
    unit->runtime_initialized_ = true;
}

} // namespace defn