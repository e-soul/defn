// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hostile_scaling.h"

#include <algorithm>
#include <cmath>

namespace defn {

namespace {

// Rounds to the nearest point, but never rounds an attack away entirely: a unit that had a weapon keeps it. Scales
// below 1 are not expected -- the mode only ever ramps up -- but a scale is a number a data file can set, and a
// hostile that silently stops hitting is a far worse failure than one that hits for 1.
int scale_damage(int damage, double scale) {
    if (damage <= 0) {
        return damage;
    }
    return std::max(1, static_cast<int>(std::lround(static_cast<double>(damage) * scale)));
}

// Same floor, same reasoning: a body with no hit points at all is dead on arrival rather than weak.
int scale_hp(int hit_points, double scale) {
    if (hit_points <= 0) {
        return hit_points;
    }
    return std::max(1, static_cast<int>(std::lround(static_cast<double>(hit_points) * scale)));
}

} // namespace

UnitConfig with_hostile_scale(const UnitConfig &config, const HostileScale &scale) {
    if (scale.is_identity()) {
        return config;
    }

    UnitConfig scaled = config;
    scaled.hp = scale_hp(config.hp, scale.hp);
    scaled.scale = static_cast<float>(static_cast<double>(config.scale) * scale.size);
    scaled.melee_damage = scale_damage(config.melee_damage, scale.damage);
    scaled.ranged_damage = scale_damage(config.ranged_damage, scale.damage);
    if (scaled.projectile_attack.has_value()) {
        ProjectileAttackConfig &projectile = *scaled.projectile_attack;
        if (projectile.impact_damage.has_value()) {
            projectile.impact_damage = scale_damage(*projectile.impact_damage, scale.damage);
        }
        if (projectile.splash_damage.has_value()) {
            projectile.splash_damage = scale_damage(*projectile.splash_damage, scale.damage);
        }
    }
    return scaled;
}

} // namespace defn
