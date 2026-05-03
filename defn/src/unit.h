#ifndef UNIT_H
#define UNIT_H

#include "battle_entity.h"
#include "unit_data.h"
#include "unit_runtime_profile.h"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class HealthComponent;
class HealthBarWidget;
class AnimationController;
class CombatComponent;
class DetectionComponent;
class HitboxComponent;
class MovementComponent;
class SoundController;
class UnitFactory;

class Unit : public BattleEntity {
  GDCLASS(Unit, BattleEntity)

  public:
    Unit();
    ~Unit() override = default;

    void set_unit_config(const UnitConfig &cfg);
    void set_runtime_profile(const UnitRuntimeProfile &profile) { runtime_profile_ = profile; }

    void flash_damage(const Color &color) override;

    int get_cost() const { return unit_config_.cost; }
    int get_bounty() const { return unit_config_.bounty; }
    const UnitConfig &get_unit_config() const { return unit_config_; }
    const UnitRuntimeProfile &get_runtime_profile() const { return runtime_profile_; }
    real_t get_attack_range() const { return attack_range; }
    real_t get_ranged_range() const { return ranged_range; }
    MovementComponent *get_movement_component() const override { return movement; }

    void _ready() override;

  protected:
    static void _bind_methods();

  private:
    friend class UnitFactory;

    void on_died();
    void attach_health_component(HealthComponent *health_component) { set_health_component(health_component); }
    void attach_hitbox_component(HitboxComponent *hitbox_component) { set_hitbox_component(hitbox_component); }

    UnitConfig unit_config_;
    UnitRuntimeProfile runtime_profile_{};
    real_t attack_range = 128.0;
    real_t ranged_range = 384.0;
    bool runtime_initialized_ = false;

    HealthComponent *health = nullptr;
    HealthBarWidget *health_bar_widget = nullptr;
    AnimationController *animation = nullptr;
    CombatComponent *combat = nullptr;
    DetectionComponent *detection = nullptr;
    MovementComponent *movement = nullptr;
    SoundController *sound = nullptr;
};

} // namespace defn

#endif
