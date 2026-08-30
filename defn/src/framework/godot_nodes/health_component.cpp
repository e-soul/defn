// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "health_component.h"

#include "damage_rules.h"

#include <algorithm>

namespace defn {

void HealthComponent::_bind_methods() {
    ADD_SIGNAL(MethodInfo("died"));
    ADD_SIGNAL(MethodInfo("health_changed", PropertyInfo(Variant::INT, "current"), PropertyInfo(Variant::INT, "max")));
}

void HealthComponent::configure(int p_max_hp, int p_armour, int p_damage_cap) {
    max_hp = p_max_hp;
    current_hp = max_hp;
    armour = p_armour;
    damage_cap = p_damage_cap;
    emit_signal("health_changed", current_hp, max_hp);
}

void HealthComponent::set_max_hp_and_heal(int new_max_hp) {
    max_hp = std::max(new_max_hp, 0);
    current_hp = max_hp;
    emit_signal("health_changed", current_hp, max_hp);
}

int HealthComponent::take_damage(int amount) {
    amount = std::max(amount, 0);
    if (is_dead() || amount == 0) {
        return 0;
    }

    // Same rule, same place in the sequence as SimWorld::take_damage. Conformance compares the two.
    amount = damage_after_mitigation(amount, damage_cap, armour);

    const int previous_hp = current_hp;
    current_hp -= amount;
    current_hp = std::max(current_hp, 0);
    emit_signal("health_changed", current_hp, max_hp);

    if (current_hp <= 0) {
        emit_signal("died");
    }
    return previous_hp - current_hp;
}

} // namespace defn
