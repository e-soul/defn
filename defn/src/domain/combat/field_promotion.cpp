#include "field_promotion.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace defn {

FieldPromotionTransition record_field_promotion_damage(const FieldPromotionState &state, int effective_damage, const FieldPromotionRules &rules) {
    FieldPromotionTransition result{.state = state};
    if (state.promoted || effective_damage <= 0 || rules.damage_threshold <= 0) {
        return result;
    }

    const long long accumulated = static_cast<long long>(state.effective_damage_dealt) + effective_damage;
    result.state.effective_damage_dealt = static_cast<int>(std::min(accumulated, static_cast<long long>(std::numeric_limits<int>::max())));
    if (result.state.effective_damage_dealt >= rules.damage_threshold) {
        result.state.promoted = true;
        result.promotion_granted = true;
    }
    return result;
}

int apply_promoted_damage(int base_damage, const FieldPromotionRules &rules) {
    if (base_damage <= 0) {
        return 0;
    }
    const double scaled = static_cast<double>(base_damage) * rules.damage_multiplier;
    const long long rounded = std::lround(scaled);
    return static_cast<int>(std::clamp(rounded, 1LL, static_cast<long long>(std::numeric_limits<int>::max())));
}

double apply_promoted_attack_period(double base_period_seconds, const FieldPromotionRules &rules) {
    return base_period_seconds > 0.0 ? base_period_seconds * rules.attack_period_multiplier : 0.0;
}

int apply_promoted_max_health(int base_max_health, const FieldPromotionRules &rules) {
    if (base_max_health <= 0) {
        return 0;
    }
    const long long rounded = std::lround(static_cast<double>(base_max_health) * rules.health_multiplier);
    return static_cast<int>(std::clamp(rounded, 1LL, static_cast<long long>(std::numeric_limits<int>::max())));
}

} // namespace defn
