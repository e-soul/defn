#ifndef COMBAT_RUNTIME_H
#define COMBAT_RUNTIME_H

#include "attack_target.h"
#include "combat_attack_executor.h"
#include "combat_logic.h"
#include "combat_types.h"

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class BattleEntity;
class AnimationController;
class HealthComponent;
enum class AnimState : int;

class CombatRuntime {
  public:
    void configure(BattleEntity *unit, HealthComponent *health, AnimationController *animation, Area2D *detection_area, const CombatConfig &config);
    void update(double delta);

    bool is_engaged() const { return state_.engaged; }
    AttackMode get_attack_mode() const { return state_.attack_mode; }

  private:
    void update_target();
    void try_spawn_pending_projectile();
    void apply_logic_step(const CombatLogicStep &step, double delta);
    static CombatPoseState map_pose_state(AnimState state);

    BattleEntity *unit_ = nullptr;
    HealthComponent *health_ = nullptr;
    AnimationController *animation_ = nullptr;
    Area2D *detection_area_ = nullptr;
    CombatConfig config_{};
    CombatTargetSelection selection_{};
    CombatLogicState state_{};
    PendingProjectileSpawn pending_projectile_{};
};

} // namespace defn

#endif