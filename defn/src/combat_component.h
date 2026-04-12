#ifndef COMBAT_COMPONENT_H
#define COMBAT_COMPONENT_H

#include "combat_runtime.h"
#include "combat_types.h"
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class Unit;
class AnimationController;
class HealthComponent;

class CombatComponent : public Node {
    GDCLASS(CombatComponent, Node)

  public:
    using Config = CombatConfig;

    void configure(Unit *p_unit, HealthComponent *p_health, AnimationController *p_anim, Area2D *p_detection_area, const Config &cfg);

    void _process(double delta) override;

    bool is_engaged() const { return runtime_.is_engaged(); }
    AttackMode get_attack_mode() const { return runtime_.get_attack_mode(); }

  protected:
    static void _bind_methods();

  private:
    CombatRuntime runtime_;
};

} // namespace defn

#endif
