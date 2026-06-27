#include "unit_runtime_config_resolver.h"

namespace defn {

ResolvedUnitRuntimeConfig resolve_unit_runtime_config(const RuntimeRangeConfig &config, RandomSource &random) {
    return {
        .melee_attack_range = config.melee_attack_range * random.range_real(config.melee_attack_range_variation_min, config.melee_attack_range_variation_max),
        .ranged_attack_range = config.ranged_attack_range * random.range_real(config.ranged_attack_range_variation_min, config.ranged_attack_range_variation_max),
    };
}

} // namespace defn