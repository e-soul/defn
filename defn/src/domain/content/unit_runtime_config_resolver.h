#ifndef UNIT_RUNTIME_CONFIG_RESOLVER_H
#define UNIT_RUNTIME_CONFIG_RESOLVER_H

#include "random_source.h"
#include "unit_definition.h"

namespace defn {

struct RuntimeRangeConfig {
    float melee_attack_range = 128.0F;
    float melee_attack_range_variation_min = 1.0F;
    float melee_attack_range_variation_max = 1.0F;
    float ranged_attack_range = 384.0F;
    float ranged_attack_range_variation_min = 1.0F;
    float ranged_attack_range_variation_max = 1.0F;
};

struct ResolvedUnitRuntimeConfig {
    float melee_attack_range = 128.0F;
    float ranged_attack_range = 384.0F;
};

[[nodiscard]] RuntimeRangeConfig to_runtime_range_config(const UnitConfig &config);
[[nodiscard]] ResolvedUnitRuntimeConfig resolve_unit_runtime_config(const RuntimeRangeConfig &config, RandomSource &random);

} // namespace defn

#endif