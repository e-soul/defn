#include "unit_progression_mapper.h"

namespace defn {

ProgressionUnitStats to_progression_unit_stats(const UnitConfig &config) {
    return {
        .unit_id = config.name,
        .friendly = config.side == UnitSide::FRIENDLY,
        .hp = config.hp,
        .ranged_damage = config.ranged_damage,
        .move_speed = static_cast<float>(config.move_speed_pixels_per_second),
        .has_projectile_attack = config.projectile_attack.has_value(),
    };
}

UnitConfig with_progression_unit_stats(UnitConfig config, const ProgressionUnitStats &stats) {
    config.hp = stats.hp;
    config.ranged_damage = stats.ranged_damage;
    config.move_speed_pixels_per_second = stats.move_speed;
    return config;
}

} // namespace defn
