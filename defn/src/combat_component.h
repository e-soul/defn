#ifndef COMBAT_COMPONENT_H
#define COMBAT_COMPONENT_H

#include "unit_data.h"
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

enum class AttackMode { NONE, MELEE, RANGED };

class Unit;
class AnimationController;
class HealthComponent;

class CombatComponent : public Node {
    GDCLASS(CombatComponent, Node)

  public:
    struct Config {
        UnitSide side;
        int melee_damage;
        double melee_attack_period_seconds;
        int ranged_damage;
        double ranged_attack_period_seconds;
        double attack_range;
        double ranged_range;
        Color melee_flash_color;
        Color ranged_flash_color;
    };

    void configure(Unit *p_unit, HealthComponent *p_health, AnimationController *p_anim, Area2D *p_detection_area, const Config &cfg);

    void _process(double delta) override;

    bool is_engaged() const { return engaged; }
    AttackMode get_attack_mode() const { return attack_mode; }

  protected:
    static void _bind_methods();

  private:
    void find_target();
    bool try_keep_target();
    void find_new_target();
    double get_forward_distance(Unit *other) const;
    void update_cooldowns(double delta);
    void perform_behavior(double delta);
    void check_breach();
    double get_attack_period_seconds() const;
    void trigger_attack();

    Unit *unit = nullptr;
    HealthComponent *health = nullptr;
    AnimationController *anim = nullptr;
    Area2D *detection_area = nullptr;

    Config config{};
    double attack_cooldown_seconds = 0.0;
    AttackMode attack_mode = AttackMode::NONE;
    bool engaged = false;
    Unit *target = nullptr;
};

} // namespace defn

#endif
