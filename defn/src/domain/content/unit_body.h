// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UNIT_BODY_H
#define UNIT_BODY_H

namespace defn {

// How wide a unit is to anything that touches it. Every unit carries a circle of this radius in world space whatever
// its sprite scale, and it is the only body the game has: the sensor that acquires a target overlaps it, and the
// camera trigger a unit walks into overlaps it too.
//
// It lives here, at the innermost layer, because both of those readers are outside the domain and on opposite sides
// of the engine boundary -- `UnitFactory` builds the shape for the shipped game, `SimWorld` and `SimCamera` model it
// for the kernel -- and a body the two sides size differently is a body they disagree about.
inline constexpr float UNIT_HITBOX_RADIUS = 5.0F;

} // namespace defn

#endif
