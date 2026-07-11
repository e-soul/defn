#include "combat_component.h"

#include "battle_entity.h"

namespace defn {

void CombatComponent::_bind_methods() {}

void CombatComponent::configure(BattleEntity *p_unit, HealthComponent *p_health, AnimationController *p_anim, Area2D *p_detection_area, const Config &cfg,
                                const std::optional<ProjectileAttackConfig> &projectile_attack) {
    runtime_.configure(p_unit, p_health, p_anim, p_detection_area, cfg, projectile_attack);
    set_enabled(true);
}

void CombatComponent::_process(double delta) {
    if (!enabled_) {
        return;
    }

    runtime_.update(delta);
}

void CombatComponent::set_enabled(bool enabled) {
    enabled_ = enabled;
    set_process(enabled);
}

void CombatComponent::apply_field_promotion(const FieldPromotionRules &rules) { runtime_.apply_field_promotion(rules); }

} // namespace defn
