#include "combat_component.h"

namespace defn {

void CombatComponent::_bind_methods() {}

void CombatComponent::configure(Unit *p_unit, HealthComponent *p_health, AnimationController *p_anim, Area2D *p_detection_area, const Config &cfg) {
    runtime_.configure(p_unit, p_health, p_anim, p_detection_area, cfg);
}

void CombatComponent::_process(double delta) { runtime_.update(delta); }

} // namespace defn
