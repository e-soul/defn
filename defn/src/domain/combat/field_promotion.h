#ifndef FIELD_PROMOTION_H
#define FIELD_PROMOTION_H

namespace defn {

struct FieldPromotionRules {
    int damage_threshold = 500;
    double damage_multiplier = 1.10;
    double attack_period_multiplier = 0.90;
    double health_multiplier = 1.10;
};

struct FieldPromotionState {
    int effective_damage_dealt = 0;
    bool promoted = false;
};

struct FieldPromotionTransition {
    FieldPromotionState state;
    bool promotion_granted = false;
};

[[nodiscard]] FieldPromotionTransition record_field_promotion_damage(const FieldPromotionState &state, int effective_damage, const FieldPromotionRules &rules);
[[nodiscard]] int apply_promoted_damage(int base_damage, const FieldPromotionRules &rules);
[[nodiscard]] double apply_promoted_attack_period(double base_period_seconds, const FieldPromotionRules &rules);
[[nodiscard]] int apply_promoted_max_health(int base_max_health, const FieldPromotionRules &rules);

} // namespace defn

#endif
