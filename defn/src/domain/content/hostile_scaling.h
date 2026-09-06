// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HOSTILE_SCALING_H
#define HOSTILE_SCALING_H

#include "unit_definition.h"

namespace defn {

// Scales what a unit hits for, leaving everything else -- hp, reach, speed, bounty, armour -- alone.
//
// Endless mode needs difficulty that grows without growing the body count. Spending a larger budget buys more
// hostiles, and `ENDLESS_MODE.md` records where that runs out: driving the army-to-wave multiple down to 1 by wave
// size alone needs waves of order 10^21 bodies. Scaling the hostiles instead is unbounded at a constant number of
// them, and it is the only cheap lever that reaches the actual problem -- friendlies persist between waves and
// hostiles do not, so the player's line is a stock that only ever grows until something starts killing it. More
// hostile *hp* lengthens fights; more hostile *damage* removes the stock.
//
// One function, called from both spawn paths -- `SimWorld::spawn` and `UnitFactory::materialize` -- because the
// conformance suite compares them tick for tick and a scaling rule applied twice is a scaling rule applied
// differently.
[[nodiscard]] UnitConfig with_damage_scale(const UnitConfig &config, double scale);

} // namespace defn

#endif
