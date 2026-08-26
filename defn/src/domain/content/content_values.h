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

} // namespace defn

#endif