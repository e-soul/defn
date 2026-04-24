#include "unit_factory.h"

#include "animation_controller.h"
#include "collision_layers.h"
#include "combat_component.h"
#include "detection_component.h"
#include "health_bar_widget.h"
#include "health_component.h"
#include "sound_controller.h"
#include "unit.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

namespace {

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

DetectionComponent *create_detection_component(Unit *unit) {
    auto *detection = memnew(DetectionComponent);
    detection->set_name("DetectionComponent");
    unit->add_child(detection);

    const real_t scale_x = unit->get_scale().x;
    const auto detection_channels = get_detection_channels(unit->get_side());
    detection->configure(unit, detection_channels.hitbox_layer, detection_channels.sensor_mask, unit->get_ranged_range(), scale_x);
    return detection;
}

CombatComponent::Config make_combat_config(const Unit *unit) {
    const auto &config = unit->get_unit_config();
    return {
        .side = config.side,
        .melee_damage = config.melee_damage,
        .melee_attack_period_seconds = config.melee_attack_period_seconds,
        .ranged_damage = config.ranged_damage,
        .ranged_attack_period_seconds = config.ranged_attack_period_seconds,
        .attack_range = unit->get_attack_range(),
        .ranged_range = unit->get_ranged_range(),
        .melee_flash_color = config.melee_flash_color,
        .ranged_flash_color = config.ranged_flash_color,
        .projectile_attack = config.projectile_attack,
    };
}

CombatComponent *create_combat_component(Unit *unit, HealthComponent *health, AnimationController *animation, DetectionComponent *detection) {
    auto *combat = memnew(CombatComponent);
    combat->set_name("CombatComponent");
    unit->add_child(combat);
    combat->configure(unit, health, animation, detection->get_detection_area(), make_combat_config(unit));
    return combat;
}

} // namespace

Unit *UnitFactory::create(const UnitConfig &config, const Vector2 &position) {
    auto *unit = memnew(Unit);
    unit->set_unit_config(config);
    unit->set_position(position);
    return unit;
}

Unit *UnitFactory::materialize(const UnitSpawnRequest &request) { return create(request.config, request.position); }

void UnitFactory::initialize(Unit *unit) {
    if (unit == nullptr || unit->runtime_initialized_) {
        return;
    }

    unit->health = create_health_component(unit);
    unit->health->connect("died", callable_mp(unit, &Unit::on_died));
    unit->health_bar_widget = create_health_bar_widget(unit, unit->health);
    unit->animation = create_animation_controller(unit);
    unit->sound = create_sound_controller(unit, unit->animation);
    unit->detection = create_detection_component(unit);
    unit->combat = create_combat_component(unit, unit->health, unit->animation, unit->detection);
    unit->runtime_initialized_ = true;
}

} // namespace defn