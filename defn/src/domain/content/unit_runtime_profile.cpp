#include "unit_runtime_profile.h"

#include "unit_data.h"

namespace defn {

UnitRuntimeProfile UnitRuntimeProfile::from_unit_config(const UnitConfig &config) {
    UnitRuntimeProfile profile;
    const bool can_attack = config.melee_damage > 0 || config.ranged_damage > 0 || config.projectile_attack.has_value();
    profile.enable_combat = can_attack;
    profile.enable_target_sensor = can_attack;
    profile.enable_sound = can_attack && !config.shoot_sfx.path.empty();
    profile.enable_movement = config.move_speed_pixels_per_second > 0.0F;
    return profile;
}

} // namespace defn