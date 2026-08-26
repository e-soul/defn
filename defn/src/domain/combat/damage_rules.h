// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DAMAGE_RULES_H
#define DAMAGE_RULES_H

namespace defn {

// Flat damage reduction, with a floor of one so nothing is ever completely immune.
//
// This is the cheapest available breakpoint: a discontinuity in the value function. Everything else in the rules is
// linear, and +1 damage is worth exactly +1 damage against everything, which is precisely why the payoff matrix is
// separable. Armour makes "many small shots" and "few big shots" into genuinely different things, per target -- a
// non-additivity that no amount of target selection could supply, because it changes what a shot is worth rather
// than where it lands.
//
// Deliberately its own header with no includes: it is called from the shipped game's health component, which lives
// in a translation unit where `Vector2` has to keep meaning `godot::Vector2`.
[[nodiscard]] inline int damage_after_armour(int damage, int armour) {
    if (damage <= 0) {
        return 0;
    }
    return damage > armour ? damage - armour : 1;
}

} // namespace defn

#endif
