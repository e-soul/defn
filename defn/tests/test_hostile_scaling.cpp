// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "hostile_scaling.h"

namespace defn {

namespace {

UnitConfig make_shooter() {
    UnitConfig config;
    config.name = "jackal";
    config.melee_damage = 15;
    config.ranged_damage = 8;
    config.hp = 120;
    config.armour = 3;
    config.bounty = 12;
    config.move_speed_pixels_per_second = 64.0F;
    config.scale = 0.25F;
    ProjectileAttackConfig projectile;
    projectile.impact_damage = 20;
    projectile.splash_damage = 6;
    config.projectile_attack = projectile;
    return config;
}

} // namespace

DEFN_TEST(hostile_scaling_returns_the_config_untouched_at_scale_one) {
    const UnitConfig original = make_shooter();
    const UnitConfig scaled = with_hostile_scale(original, {});

    DEFN_CHECK(scaled.melee_damage == original.melee_damage);
    DEFN_CHECK(scaled.ranged_damage == original.ranged_damage);
    DEFN_CHECK(scaled.projectile_attack->impact_damage == original.projectile_attack->impact_damage);
    DEFN_CHECK(scaled.projectile_attack->splash_damage == original.projectile_attack->splash_damage);
}

DEFN_TEST(hostile_scaling_multiplies_every_damage_channel) {
    const UnitConfig scaled = with_hostile_scale(make_shooter(), {.damage = 2.5});

    // Every way a unit deals damage has to move together, or the ramp quietly reshapes the roster: scaling melee
    // alone would turn the whole hostile side into divers as the run goes on.
    DEFN_CHECK(scaled.melee_damage == 38);
    DEFN_CHECK(scaled.ranged_damage == 20);
    DEFN_CHECK(scaled.projectile_attack->impact_damage == 50);
    DEFN_CHECK(scaled.projectile_attack->splash_damage == 15);
}

DEFN_TEST(hostile_scaling_leaves_everything_that_is_not_damage_alone) {
    const UnitConfig original = make_shooter();
    const UnitConfig scaled = with_hostile_scale(original, {.damage = 4.0});

    // The damage channel is meant to make hostiles hit harder, not to make them tougher, faster or worth more.
    DEFN_CHECK(scaled.hp == original.hp);
    DEFN_CHECK(scaled.armour == original.armour);
    DEFN_CHECK(scaled.bounty == original.bounty);
    DEFN_CHECK_CLOSE(scaled.move_speed_pixels_per_second, original.move_speed_pixels_per_second, 1e-6);
}

DEFN_TEST(hostile_scaling_multiplies_hit_points_and_the_sprite_without_touching_the_counters) {
    const UnitConfig original = make_shooter();
    const UnitConfig scaled = with_hostile_scale(original, {.hp = 3.0, .size = 1.4});

    DEFN_CHECK(scaled.hp == 360);
    DEFN_CHECK_CLOSE(scaled.scale, 0.35F, 1e-6);

    // An elite is the same unit with more of it. Armour and the damage cap decide *which* attacker can hurt this
    // body at all, and bounty decides what killing it pays -- scaling any of the three would rewrite the roster's
    // counter matrix or hand the escalation back to the player as income.
    DEFN_CHECK(scaled.armour == original.armour);
    DEFN_CHECK(scaled.damage_cap == original.damage_cap);
    DEFN_CHECK(scaled.bounty == original.bounty);
    DEFN_CHECK(scaled.melee_damage == original.melee_damage);
    DEFN_CHECK(scaled.ranged_damage == original.ranged_damage);
    DEFN_CHECK_CLOSE(scaled.move_speed_pixels_per_second, original.move_speed_pixels_per_second, 1e-6);
}

DEFN_TEST(hostile_scaling_composes_a_wave_scale_with_a_body_scale) {
    // The wave-wide ramp and the per-body elite promotion are the same kind of thing at two granularities, so they
    // multiply. If one overrode the other, an elite in a damage-ramped wave would silently lose the ramp.
    const HostileScale composed = HostileScale{.hp = 3.0, .size = 1.4}.composed_with({.damage = 2.0, .hp = 1.5});

    DEFN_CHECK_CLOSE(composed.damage, 2.0, 1e-9);
    DEFN_CHECK_CLOSE(composed.hp, 4.5, 1e-9);
    DEFN_CHECK_CLOSE(composed.size, 1.4, 1e-9);
}

DEFN_TEST(hostile_scaling_never_rounds_a_body_away) {
    UnitConfig frail;
    frail.hp = 3;
    const UnitConfig scaled = with_hostile_scale(frail, {.hp = 0.01});

    DEFN_CHECK(scaled.hp == 1);
}

DEFN_TEST(hostile_scaling_never_rounds_an_attack_away) {
    UnitConfig weak;
    weak.melee_damage = 1;
    weak.ranged_damage = 0; // no gun at all, and it must not acquire one
    const UnitConfig scaled = with_hostile_scale(weak, {.damage = 0.1});

    DEFN_CHECK(scaled.melee_damage == 1);
    DEFN_CHECK(scaled.ranged_damage == 0);
}

} // namespace defn
