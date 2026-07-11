#include "field_promotion.h"
#include "field_promotion_runtime.h"
#include "test_harness.h"

namespace defn {

DEFN_TEST(field_promotion_runtime_accumulates_and_grants_once) {
    FieldPromotionRuntime runtime;
    runtime.configure({}, true);

    DEFN_CHECK_EQ(runtime.get_effective_damage_dealt(), 0);
    DEFN_CHECK(!runtime.is_promoted());
    DEFN_CHECK(!runtime.record_effective_damage(200).promotion_granted);
    DEFN_CHECK_EQ(runtime.get_effective_damage_dealt(), 200);
    DEFN_CHECK(!runtime.record_effective_damage(299).promotion_granted);
    DEFN_CHECK(runtime.record_effective_damage(1).promotion_granted);
    DEFN_CHECK(runtime.is_promoted());
    DEFN_CHECK(!runtime.record_effective_damage(500).promotion_granted);
}

DEFN_TEST(field_promotion_runtime_crossing_threshold_and_invalid_damage) {
    FieldPromotionRuntime runtime;
    runtime.configure({}, true);
    DEFN_CHECK(!runtime.record_effective_damage(0).promotion_granted);
    DEFN_CHECK(!runtime.record_effective_damage(-20).promotion_granted);
    DEFN_CHECK(runtime.record_effective_damage(650).promotion_granted);
    DEFN_CHECK_EQ(runtime.get_effective_damage_dealt(), 650);
}

DEFN_TEST(field_promotion_runtime_ignores_ineligible_units) {
    FieldPromotionRuntime runtime;
    runtime.configure({}, false);
    DEFN_CHECK(!runtime.record_effective_damage(1000).promotion_granted);
    DEFN_CHECK_EQ(runtime.get_effective_damage_dealt(), 0);
    DEFN_CHECK(!runtime.is_promoted());
}

DEFN_TEST(field_promotion_helpers_round_and_preserve_positive_damage) {
    const FieldPromotionRules rules{};
    DEFN_CHECK_EQ(apply_promoted_damage(10, rules), 11);
    DEFN_CHECK_EQ(apply_promoted_damage(5, rules), 6);
    DEFN_CHECK_EQ(apply_promoted_damage(1, {.damage_multiplier = 0.01}), 1);
    DEFN_CHECK_EQ(apply_promoted_damage(0, rules), 0);
    DEFN_CHECK_EQ(apply_promoted_damage(-4, rules), 0);
    DEFN_CHECK_CLOSE(apply_promoted_attack_period(1.0, rules), 0.9, 0.000001);
    DEFN_CHECK_CLOSE(apply_promoted_attack_period(2.0, rules), 1.8, 0.000001);
    DEFN_CHECK_EQ(apply_promoted_max_health(95, rules), 105);
    DEFN_CHECK_EQ(apply_promoted_max_health(0, rules), 0);
}

DEFN_TEST(field_promotion_runtime_is_instance_local) {
    FieldPromotionRuntime promoted;
    promoted.configure({}, true);
    DEFN_CHECK(promoted.record_effective_damage(500).promotion_granted);

    FieldPromotionRuntime fresh;
    fresh.configure({}, true);
    DEFN_CHECK(!fresh.is_promoted());
    DEFN_CHECK_EQ(fresh.get_effective_damage_dealt(), 0);
    DEFN_CHECK_EQ(fresh.outgoing_damage(10), 10);
    DEFN_CHECK_EQ(promoted.outgoing_damage(10), 11);
}

} // namespace defn
