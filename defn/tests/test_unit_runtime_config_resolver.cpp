#include "test_harness.h"

#include "scripted_random_source.h"
#include "unit_runtime_config_resolver.h"

namespace defn {

DEFN_TEST(unit_runtime_config_resolver_applies_scripted_range_variation) {
    tests::ScriptedRandomSource random;
    random.push_real(0.75F);
    random.push_real(1.25F);

    const ResolvedUnitRuntimeConfig resolved = resolve_unit_runtime_config(
        {
            .melee_attack_range = 100.0F,
            .melee_attack_range_variation_min = 0.5F,
            .melee_attack_range_variation_max = 1.5F,
            .ranged_attack_range = 200.0F,
            .ranged_attack_range_variation_min = 0.5F,
            .ranged_attack_range_variation_max = 1.5F,
        },
        random);

    DEFN_CHECK_CLOSE(resolved.melee_attack_range, 75.0, 0.001);
    DEFN_CHECK_CLOSE(resolved.ranged_attack_range, 250.0, 0.001);
}

} // namespace defn