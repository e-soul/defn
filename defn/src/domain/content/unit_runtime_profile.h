#ifndef UNIT_RUNTIME_PROFILE_H
#define UNIT_RUNTIME_PROFILE_H

namespace defn {

struct UnitConfig;

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

    static UnitRuntimeProfile from_unit_config(const UnitConfig &config);
};

} // namespace defn

#endif