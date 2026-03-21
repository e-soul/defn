#ifndef UNIT_H
#define UNIT_H

#include "unit_data.h"
#include <godot_cpp/classes/character_body2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class HealthComponent;
class HealthBarWidget;
class AnimationController;
class CombatComponent;
class DetectionComponent;

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

    void do_movement(double delta);
    void notify_breach();

    void _ready() override;

  protected:
    static void _bind_methods();

  private:
    void on_died();

    UnitConfig unit_config_;
    double attack_range = 128.0;
    double ranged_range = 384.0;

    HealthComponent *health = nullptr;
    HealthBarWidget *health_bar_widget = nullptr;
    AnimationController *animation = nullptr;
    CombatComponent *combat = nullptr;
    DetectionComponent *detection = nullptr;
};

} // namespace defn

#endif
