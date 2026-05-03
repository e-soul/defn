#include "test_harness.h"

#include "unit_runtime_profile.h"

namespace defn {

DEFN_TEST(unit_runtime_profile_recognizes_mobile_combatant) {
    UnitConfig config;
    config.melee_damage = 15;
    config.ranged_damage = 8;
    config.move_speed_pixels_per_second = 72.0F;
    config.shoot_sfx.path = "res://assets/sfx/test.wav";

    const UnitRuntimeProfile profile = UnitRuntimeProfile::from_unit_config(config);

    DEFN_CHECK(profile.enable_combat);
    DEFN_CHECK(profile.enable_target_sensor);
    DEFN_CHECK(profile.enable_sound);
    DEFN_CHECK(profile.enable_movement);
}

DEFN_TEST(unit_runtime_profile_recognizes_stationary_combatant) {
    UnitConfig config;
    config.melee_damage = 15;
    config.ranged_damage = 0;
    config.move_speed_pixels_per_second = 0.0F;

    const UnitRuntimeProfile profile = UnitRuntimeProfile::from_unit_config(config);

    DEFN_CHECK(profile.enable_combat);
    DEFN_CHECK(profile.enable_target_sensor);
    DEFN_CHECK(!profile.enable_sound);
    DEFN_CHECK(!profile.enable_movement);
}

DEFN_TEST(unit_runtime_profile_recognizes_passive_mover) {
    UnitConfig config;
    config.melee_damage = 0;
    config.ranged_damage = 0;
    config.projectile_attack.reset();
    config.move_speed_pixels_per_second = 36.0F;
    config.shoot_sfx.path = "res://assets/sfx/test.wav";

    const UnitRuntimeProfile profile = UnitRuntimeProfile::from_unit_config(config);

    DEFN_CHECK(!profile.enable_combat);
    DEFN_CHECK(!profile.enable_target_sensor);
    DEFN_CHECK(!profile.enable_sound);
    DEFN_CHECK(profile.enable_movement);
}

DEFN_TEST(unit_runtime_profile_recognizes_passive_static) {
    UnitConfig config;
    config.melee_damage = 0;
    config.ranged_damage = 0;
    config.projectile_attack.reset();
    config.move_speed_pixels_per_second = 0.0F;

    const UnitRuntimeProfile profile = UnitRuntimeProfile::from_unit_config(config);

    DEFN_CHECK(!profile.enable_combat);
    DEFN_CHECK(!profile.enable_target_sensor);
    DEFN_CHECK(!profile.enable_sound);
    DEFN_CHECK(!profile.enable_movement);
}

DEFN_TEST(unit_runtime_profile_treats_projectiles_as_attack_capability) {
    UnitConfig config;
    config.melee_damage = 0;
    config.ranged_damage = 0;
    config.move_speed_pixels_per_second = 0.0F;
    config.projectile_attack = ProjectileAttackConfig{};

    const UnitRuntimeProfile profile = UnitRuntimeProfile::from_unit_config(config);

    DEFN_CHECK(profile.enable_combat);
    DEFN_CHECK(profile.enable_target_sensor);
    DEFN_CHECK(!profile.enable_sound);
    DEFN_CHECK(!profile.enable_movement);
}

} // namespace defn