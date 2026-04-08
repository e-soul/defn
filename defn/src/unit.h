#ifndef UNIT_H
#define UNIT_H

#include "unit_data.h"
#include <godot_cpp/classes/character_body2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class HealthComponent;
class HealthBarWidget;
class AnimationController;
class CombatComponent;
class DetectionComponent;
class SoundController;
class UnitFactory;

class Unit : public CharacterBody2D {
    GDCLASS(Unit, CharacterBody2D)

  public:
    Unit();
    ~Unit() override = default;

    void set_unit_config(const UnitConfig &cfg);

    void take_damage(int amount);
    void flash_damage(const Color &color);
    bool is_dead() const;

    int get_cost() const { return unit_config_.cost; }
    int get_bounty() const { return unit_config_.bounty; }
    UnitSide get_side() const { return unit_config_.side; }
    const UnitConfig &get_unit_config() const { return unit_config_; }
    real_t get_attack_range() const { return attack_range; }
    real_t get_ranged_range() const { return ranged_range; }

    void do_movement(double delta);
    void notify_breach();

    void _ready() override;

  protected:
    static void _bind_methods();

  private:
    friend class UnitFactory;

    void on_died();

    UnitConfig unit_config_;
    real_t attack_range = 128.0;
    real_t ranged_range = 384.0;
    bool runtime_initialized_ = false;

    HealthComponent *health = nullptr;
    HealthBarWidget *health_bar_widget = nullptr;
    AnimationController *animation = nullptr;
    CombatComponent *combat = nullptr;
    DetectionComponent *detection = nullptr;
    SoundController *sound = nullptr;
};

} // namespace defn

#endif
