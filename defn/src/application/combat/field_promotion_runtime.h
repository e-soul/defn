#ifndef FIELD_PROMOTION_RUNTIME_H
#define FIELD_PROMOTION_RUNTIME_H

#include "field_promotion.h"

namespace defn {

struct FieldPromotionUpdate {
    bool promotion_granted = false;
};

class FieldPromotionRuntime {
  public:
    void configure(const FieldPromotionRules &rules, bool eligible);
    [[nodiscard]] FieldPromotionUpdate record_effective_damage(int effective_damage);

    [[nodiscard]] bool is_eligible() const { return eligible_; }
    [[nodiscard]] bool is_promoted() const { return state_.promoted; }
    [[nodiscard]] int get_effective_damage_dealt() const { return state_.effective_damage_dealt; }
    [[nodiscard]] int outgoing_damage(int base_damage) const;
    [[nodiscard]] const FieldPromotionRules &get_rules() const { return rules_; }

  private:
    FieldPromotionRules rules_{};
    FieldPromotionState state_{};
    bool eligible_ = false;
};

} // namespace defn

#endif
