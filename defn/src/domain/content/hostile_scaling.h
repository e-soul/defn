// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HOSTILE_SCALING_H
#define HOSTILE_SCALING_H

#include "unit_definition.h"

namespace defn {

// What a spawned hostile is multiplied by, relative to its catalog entry. Identity everywhere except in a mode that
// escalates, and applied through one shared function so both spawn paths agree tick for tick.
//
// The three components are not interchangeable, and `ENDLESS_MODE.md` records why.
//
// `hp` is the escalation lever that works. It lengthens the fight at the battle line without adding bodies to it,
// which is the wall a pure budget ramp runs into -- driving the army-to-wave multiple to 1 by wave size alone needs
// waves of order 10^21. It is also the only one of the three that leaves the roster's counter structure exactly
// where it was measured: every attacker's time-to-kill is multiplied by the same factor, so what counters what does
// not move. And it throttles income on its own, because a kill pays the same bounty for more seconds of work.
//
// `damage` is measured and is knife-edge: eight times hostile damage for thirty-seven waves changes nothing, and
// 2.86x at wave 5 ends the run, because it crosses one-shot thresholds rather than scaling smoothly. It is left in
// because the ramp exists and is tested, not because it is a good difficulty curve. Default it off.
//
// `size` is presentation. It is what makes an elite legible on screen before it is in contact, and it is a category
// marker rather than a readout: a player reads "that one is bigger, it will take longer", not a multiple. Ranges and
// hitboxes are compensated for sprite scale on the Godot side, so this changes what is drawn and where the muzzle
// sits -- which the kernel computes from the same scaled config -- and nothing else.
struct HostileScale {
    double damage = 1.0;
    double hp = 1.0;
    double size = 1.0;

    [[nodiscard]] bool is_identity() const { return damage == 1.0 && hp == 1.0 && size == 1.0; }

    // Component-wise product. A spawn's own scale composed with the scale its wave carries: the wave-wide ramp and
    // the per-body elite promotion are the same kind of thing at two granularities, so they multiply rather than
    // one overriding the other.
    [[nodiscard]] HostileScale composed_with(const HostileScale &other) const {
        return {.damage = damage * other.damage, .hp = hp * other.hp, .size = size * other.size};
    }
};

// Scales what a unit hits for, how much of it there is, and how large it is drawn. Everything else -- reach, speed,
// bounty, armour, the damage cap -- is left alone.
//
// Armour and the damage cap are deliberately not scaled. Both are flat rules that decide *which* attacker can hurt
// this unit at all, so scaling them would rewrite the counter matrix the roster is measured on; `hp` multiplies
// every attacker's work by the same factor and leaves that matrix alone. Bounty is not scaled either: an elite
// paying a normal bounty for several times the work is the income throttle, and paying it in proportion would hand
// the escalation straight back to the player as energy.
//
// One function, called from both spawn paths -- `SimWorld::spawn` and `UnitFactory::materialize` -- because the
// conformance suite compares them tick for tick and a scaling rule applied twice is a scaling rule applied
// differently.
[[nodiscard]] UnitConfig with_hostile_scale(const UnitConfig &config, const HostileScale &scale);

} // namespace defn

#endif
