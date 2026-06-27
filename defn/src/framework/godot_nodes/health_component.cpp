#include "health_component.h"
#include <algorithm>

namespace defn {

void HealthComponent::_bind_methods() {
    ADD_SIGNAL(MethodInfo("died"));
    ADD_SIGNAL(MethodInfo("health_changed", PropertyInfo(Variant::INT, "current"), PropertyInfo(Variant::INT, "max")));
}

void HealthComponent::configure(int p_max_hp) {
    max_hp = p_max_hp;
    current_hp = max_hp;
    emit_signal("health_changed", current_hp, max_hp);
}

void HealthComponent::take_damage(int amount) {
    if (is_dead()) {
        return;
    }

    current_hp -= amount;
    current_hp = std::max(current_hp, 0);
    emit_signal("health_changed", current_hp, max_hp);

    if (current_hp <= 0) {
        emit_signal("died");
    }
}

} // namespace defn
