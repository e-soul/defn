#ifndef COMBAT_RUNTIME_H
#define COMBAT_RUNTIME_H

#include "unit_data.h"

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object_id.hpp>

namespace defn {

using namespace godot;

enum class AttackMode { NONE, MELEE, RANGED };

struct CombatConfig {
    UnitSide side = UnitSide::FRIENDLY;
    int melee_damage = 0;
    double melee_attack_period_seconds = 0.0;
    int ranged_damage = 0;
    double ranged_attack_period_seconds = 0.0;
    real_t attack_range = 0.0F;
    real_t ranged_range = 0.0F;
    Color melee_flash_color;
    Color ranged_flash_color;
    std::optional<ProjectileAttackConfig> projectile_attack;
};

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
    struct PendingProjectileSpawn {
        bool active = false;
        ObjectID target_id{};
        Vector2 target_global_position;
    };

    struct State {
        double attack_cooldown_seconds = 0.0;
        AttackMode attack_mode = AttackMode::NONE;
        bool engaged = false;
        Node2D *target = nullptr;
        PendingProjectileSpawn pending_projectile{};
    };

    void update_cooldowns(double delta);
    void update_target();
    bool try_keep_target();
    void find_new_target();
    real_t get_forward_distance(Node2D *other) const;
    void try_spawn_pending_projectile();
    void perform_behavior(double delta);
    void reset_engagement();
    double get_attack_period_seconds() const;
    void trigger_attack();

    Unit *unit_ = nullptr;
    HealthComponent *health_ = nullptr;
    AnimationController *animation_ = nullptr;
    Area2D *detection_area_ = nullptr;
    CombatConfig config_{};
    State state_{};
};

} // namespace defn

#endif