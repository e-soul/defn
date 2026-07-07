#ifndef PROGRESSION_SERVICE_H
#define PROGRESSION_SERVICE_H

#include "progression_models.h"
#include "progression_rules.h"

#include <string>
#include <vector>

namespace defn {

// Abstract interface to the campaign progression state: unlocks, score,
// owned upgrades and the gameplay modifiers they produce.
class ProgressionService {
  public:
    virtual ~ProgressionService() = default;

    // Queries
    [[nodiscard]] virtual int get_total_score() const = 0;
    [[nodiscard]] virtual std::vector<std::string> get_unlocked_units() const = 0;
    [[nodiscard]] virtual bool is_level_completed(const std::string &level_id) const = 0;
    [[nodiscard]] virtual bool is_level_unlocked(const std::string &level_id) const = 0;
    [[nodiscard]] virtual bool can_claim_level_upgrade(const std::string &level_id) const = 0;
    [[nodiscard]] virtual bool can_claim_rescue_draft(const std::string &level_id) const = 0;
    [[nodiscard]] virtual std::string get_frontier_level_id() const = 0;
    [[nodiscard]] virtual int get_highest_level_score(const std::string &level_id) const = 0;
    [[nodiscard]] virtual std::string get_current_level_id() const = 0;
    [[nodiscard]] virtual std::vector<ProgressionLevelUnlock> get_level_unlock_data() const = 0;

    // Effective gameplay modifiers derived from owned upgrades
    [[nodiscard]] virtual ProgressionUnitStats get_effective_friendly_unit_stats(const ProgressionUnitStats &base_stats) const = 0;
    [[nodiscard]] virtual int get_effective_starting_energy(int base) const = 0;
    [[nodiscard]] virtual int get_effective_energy_regen() const = 0;
    [[nodiscard]] virtual float get_effective_bounty_multiplier() const = 0;
    [[nodiscard]] virtual int get_effective_base_integrity(int base) const = 0;

    // Progression use-case outputs and presentation models
    virtual bool select_level(const std::string &level_id) = 0;
    [[nodiscard]] virtual ProgressionMatchResult complete_level(const std::string &level_id, int level_score, bool victory) = 0;
    virtual bool claim_upgrade(const ProgressionRewardClaim &claim) = 0;
    [[nodiscard]] virtual std::vector<std::string> build_new_unlock_descriptions(const std::vector<std::string> &level_ids) const = 0;
    [[nodiscard]] virtual ProgressionRewardViewModel build_reward_view_model(const ProgressionRewardDraft &draft) const = 0;
    [[nodiscard]] virtual std::vector<ProgressionUpgradeCardViewModel> build_owned_upgrade_cards() const = 0;
};

} // namespace defn

#endif
