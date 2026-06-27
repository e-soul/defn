#ifndef UNIT_RUNTIME_PROFILE_H
#define UNIT_RUNTIME_PROFILE_H

#include "unit_data.h"

namespace defn {

struct UnitRuntimeProfile {
    bool enable_health_bar = true;
    bool enable_target_sensor = true;
    bool enable_combat = true;
    bool enable_sound = true;
    bool enable_movement = true;

    static UnitRuntimeProfile combatant() { return {}; }

    static UnitRuntimeProfile stationary_combatant() {
        UnitRuntimeProfile profile;
        profile.enable_movement = false;
        return profile;
    }

    static UnitRuntimeProfile passive_mover() {
        UnitRuntimeProfile profile;
        profile.enable_combat = false;
        profile.enable_target_sensor = false;
        profile.enable_sound = false;
        return profile;
    }

    static UnitRuntimeProfile passive_static() {
        UnitRuntimeProfile profile = passive_mover();
        profile.enable_movement = false;
        return profile;
    }

    static UnitRuntimeProfile from_unit_config(const UnitConfig &config) {
        UnitRuntimeProfile profile;
        const bool can_attack = config.melee_damage > 0 || config.ranged_damage > 0 || config.projectile_attack.has_value();
        profile.enable_combat = can_attack;
        profile.enable_target_sensor = can_attack;
        profile.enable_sound = can_attack && !config.shoot_sfx.path.is_empty();
        profile.enable_movement = config.move_speed_pixels_per_second > 0.0F;
        return profile;
    }
};

} // namespace defn

#endif