// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_PROGRESSION_H
#define SIM_PROGRESSION_H

#include "progression_service.h"

#include <string>
#include <vector>

namespace defn {

// A `ProgressionService` for one hypothetical save: a set of owned upgrades and the roster they unlock.
//
// Everything a match actually reads -- starting energy, regen, bounty multiplier, base integrity, per-unit stat
// deltas, the available roster -- is answered by the same `progression_rules` functions the shipped campaign uses, so
// a sweep over upgrade sets measures the real effects. The reward and presentation half of the port is stubbed: the
// simulator never draws a card, it only plays the match that would have earned one.
class SimProgression final : public ProgressionService {
  public:
    void configure(std::vector<std::string> base_unit_ids, std::vector<ProgressionUpgradeCard> upgrade_cards, const std::vector<std::string> &owned_upgrades);

    [[nodiscard]] int get_total_score() const override { return profile_.total_score; }
    [[nodiscard]] std::vector<std::string> get_unlocked_units() const override;
    [[nodiscard]] bool is_level_completed(const std::string &level_id) const override;
    [[nodiscard]] bool is_level_unlocked(const std::string & /*level_id*/) const override { return true; }
    [[nodiscard]] bool can_claim_level_upgrade(const std::string & /*level_id*/) const override { return false; }
    [[nodiscard]] bool can_claim_rescue_draft(const std::string & /*level_id*/) const override { return false; }
    [[nodiscard]] std::string get_frontier_level_id() const override { return current_level_id_; }
    [[nodiscard]] int get_highest_level_score(const std::string &level_id) const override;
    [[nodiscard]] std::string get_current_level_id() const override { return current_level_id_; }
    [[nodiscard]] std::vector<ProgressionLevelUnlock> get_level_unlock_data() const override { return {}; }

    [[nodiscard]] ProgressionUnitStats get_effective_friendly_unit_stats(const ProgressionUnitStats &base_stats) const override;
    [[nodiscard]] int get_effective_starting_energy(int base) const override;
    [[nodiscard]] int get_effective_energy_regen() const override;
    [[nodiscard]] float get_effective_bounty_multiplier() const override;
    [[nodiscard]] int get_effective_base_integrity(int base) const override;

    bool select_level(const std::string &level_id) override;
    [[nodiscard]] ProgressionMatchResult complete_level(const std::string &level_id, int level_score, bool victory) override;
    bool claim_upgrade(const ProgressionRewardClaim & /*claim*/) override { return true; }
    [[nodiscard]] std::vector<std::string> build_new_unlock_descriptions(const std::vector<std::string> & /*level_ids*/) const override { return {}; }
    [[nodiscard]] ProgressionRewardViewModel build_reward_view_model(const ProgressionRewardDraft & /*draft*/) const override { return {}; }
    [[nodiscard]] std::vector<ProgressionUpgradeCardViewModel> build_owned_upgrade_cards() const override { return {}; }

  private:
    ProgressionProfile profile_;
    std::vector<std::string> base_unit_ids_;
    std::vector<ProgressionUpgradeCard> upgrade_cards_;
    std::string current_level_id_ = "level_01";
};

} // namespace defn

#endif
