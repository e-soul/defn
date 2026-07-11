#ifndef COMBAT_RUNTIME_H
#define COMBAT_RUNTIME_H

#include "combat_attack_executor.h"
#include "combat_logic.h"
#include "combat_types.h"
#include "combat_use_cases.h"
#include "field_promotion.h"
#include "unit_definition.h"

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
    void configure(BattleEntity *unit, HealthComponent *health, AnimationController *animation, Area2D *detection_area, const CombatConfig &config,
                   const std::optional<ProjectileAttackConfig> &projectile_attack = std::nullopt);
    void update(double delta);

    bool is_engaged() const { return state_.engaged; }
    AttackMode get_attack_mode() const { return state_.attack_mode; }
    void apply_field_promotion(const FieldPromotionRules &rules);
    [[nodiscard]] const CombatConfig &get_config() const { return config_; }

  private:
    void update_target();
    void try_spawn_pending_projectile();
    void apply_commands(const AdvanceCombatOutput &output, double delta);
    void apply_command(const CombatCommand &command, double delta);
    static CombatPoseState map_pose_state(AnimState state);

    BattleEntity *unit_ = nullptr;
    HealthComponent *health_ = nullptr;
    AnimationController *animation_ = nullptr;
    Area2D *detection_area_ = nullptr;
    CombatConfig config_{};
    std::optional<ProjectileAttackConfig> projectile_attack_{};
    CombatTargetSelection selection_{};
    CombatLogicState state_{};
    PendingProjectileSpawn pending_projectile_{};
};

} // namespace defn

#endif
