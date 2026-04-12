#ifndef COMBAT_RUNTIME_H
#define COMBAT_RUNTIME_H

#include "attack_target.h"
#include "combat_types.h"

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class Unit;
class AnimationController;
class HealthComponent;

class CombatRuntime {
  public:
    void configure(Unit *unit, HealthComponent *health, AnimationController *animation, Area2D *detection_area, const CombatConfig &config);
    void update(double delta);

    bool is_engaged() const { return state_.engaged; }
    AttackMode get_attack_mode() const { return state_.attack_mode; }

  private:
    struct State {
        double attack_cooldown_seconds = 0.0;
        AttackMode attack_mode = AttackMode::NONE;
        bool engaged = false;
        AttackTarget *target = nullptr;
        PendingProjectileSpawn pending_projectile{};
    };

    void update_cooldowns(double delta);
    void update_target();
    void try_spawn_pending_projectile();
    void perform_behavior(double delta);
    void reset_engagement();
    double get_attack_period_seconds() const;

    Unit *unit_ = nullptr;
    HealthComponent *health_ = nullptr;
    AnimationController *animation_ = nullptr;
    Area2D *detection_area_ = nullptr;
    CombatConfig config_{};
    State state_{};
};

} // namespace defn

#endif