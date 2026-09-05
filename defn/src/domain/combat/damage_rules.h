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

// Plating: a ceiling on what any single round delivers, and the exact inverse of armour. Armour subtracts, so it
// costs a high-rate shooter the most and leaves a heavy round nearly intact; a cap truncates, so it costs the heavy
// round everything and leaves a stream of small shots untouched.
//
// The point is not the mitigation, it is that the two sit on opposite ends of the same axis. Every armour value in
// the roster pushed the same way -- bring burst -- which made reach-and-burst the answer to every question at once.
// A capped target asks the opposite question, so a force facing both has to answer both.
//
// Zero means no cap, so a unit without the stat is untouched.
[[nodiscard]] inline int damage_after_plating(int damage, int damage_cap) {
    if (damage <= 0) {
        return 0;
    }
    return damage_cap > 0 && damage > damage_cap ? damage_cap : damage;
}

// The full mitigation sequence, in one place so the two damage paths cannot drift apart in ordering as well as in
// arithmetic. Plating first, then armour: the cap describes what the round arrives carrying, armour describes what
// the plate stops, and armour keeps the floor of one so nothing is ever completely immune.
[[nodiscard]] inline int damage_after_mitigation(int damage, int damage_cap, int armour) {
    return damage_after_armour(damage_after_plating(damage, damage_cap), armour);
}

} // namespace defn

#endif
