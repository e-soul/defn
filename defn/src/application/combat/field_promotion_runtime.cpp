#include "field_promotion_runtime.h"

namespace defn {

void FieldPromotionRuntime::configure(const FieldPromotionRules &rules, bool eligible) {
    rules_ = rules;
    eligible_ = eligible;
    state_ = {};
}

FieldPromotionUpdate FieldPromotionRuntime::record_effective_damage(int effective_damage) {
    if (!eligible_) {
        return {};
    }
    const FieldPromotionTransition transition = record_field_promotion_damage(state_, effective_damage, rules_);
    state_ = transition.state;
    return {.promotion_granted = transition.promotion_granted};
}

int FieldPromotionRuntime::outgoing_damage(int base_damage) const { return state_.promoted ? apply_promoted_damage(base_damage, rules_) : base_damage; }

} // namespace defn
