#ifndef COMBAT_COMPONENT_H
#define COMBAT_COMPONENT_H

#include "combat_runtime.h"
#include "combat_types.h"
#include "field_promotion.h"
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class BattleEntity;
class AnimationController;
class HealthComponent;

class CombatComponent : public Node {
    GDCLASS(CombatComponent, Node)

  public:
    using Config = CombatConfig;

    void configure(BattleEntity *p_unit, HealthComponent *p_health, AnimationController *p_anim, Area2D *p_detection_area, const Config &cfg,
                   const std::optional<ProjectileAttackConfig> &projectile_attack = std::nullopt);

    void _process(double delta) override;

    bool is_engaged() const { return runtime_.is_engaged(); }
    AttackMode get_attack_mode() const { return runtime_.get_attack_mode(); }
    void set_enabled(bool enabled);
    void apply_field_promotion(const FieldPromotionRules &rules);
    [[nodiscard]] const CombatConfig &get_runtime_config() const { return runtime_.get_config(); }

  protected:
    static void _bind_methods();

  private:
    CombatRuntime runtime_;
    bool enabled_ = true;
};

} // namespace defn

#endif
