// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef CONTENT_VALUES_H
#define CONTENT_VALUES_H

namespace defn {

struct Color {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

struct Vector2 {
    float x = 0.0F;
    float y = 0.0F;
};

// Which enemy a unit reaches for once more than one is in range. Lives here rather than beside `UnitConfig` because
// both the content layer and the combat rules need it, and the combat rules must not depend on the catalog.
//
// This is one of the two places non-additivity enters the rules: what a unit is worth stops being a property of the
// unit alone and starts depending on what else is on the field to shoot at.
enum class TargetPreference { NEAREST, FARTHEST, LOWEST_HP, HIGHEST_HP };

// What a unit *is*, so that another unit can prefer it without naming it. The second of the two places non-additivity
// enters the rules, and the more general one: `TargetPreference` reorders candidates by geometry or by how hurt they
// are, both of which are properties of the moment. A role is a property of the roster, so a preference expressed over
// roles makes a unit's worth depend on which *kinds* of thing the other side brought -- which is the matchup the
// payoff matrix is trying to find, stated directly rather than hoped for out of geometry.
//
// NONE is the default and is never preferred by anything, so a unit that declares no role plays exactly as it did.
enum class UnitRole { NONE, TANK, DIVER, SNIPER, ASSAULT, SPLASH, SPECIALIST, SUPPORT, STRUCTURE };

// Sized to the enum, and the reason the enum is not open-ended: a bias table is a plain array on every CombatConfig,
// so preferring a role costs a multiply and no allocation.
inline constexpr int UNIT_ROLE_COUNT = 9;

inline constexpr int unit_role_index(UnitRole role) { return static_cast<int>(role); }

} // namespace defn

#endif