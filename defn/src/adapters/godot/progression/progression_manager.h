#ifndef PROGRESSION_MANAGER_H
#define PROGRESSION_MANAGER_H

#include "progression_catalog.h"
#include "progression_rules.h"
#include "progression_save_repository.h"
#include "progression_service.h"
#include "progression_use_cases.h"
#include "random_source.h"
#include "score_screen_models.h"
#include "unit_data.h"
#include "upgrade_catalog.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>
#include <vector>

namespace defn {

using namespace godot;

struct UnitConfig;

class CampaignService : public Object, public ProgressionService {
    GDCLASS(CampaignService, Object)

  public:
    CampaignService();

    static CampaignService *get_singleton();
    static void register_singleton();
    static void unregister_singleton();

    void initialize();

    // Engine-neutral progression service
    [[nodiscard]] int get_total_score() const override { return save_data_.total_score; }
    [[nodiscard]] std::vector<std::string> get_unlocked_units() const override;
    [[nodiscard]] bool is_level_completed(const std::string &level_id) const override;
    [[nodiscard]] bool is_level_unlocked(const std::string &level_id) const override;
    [[nodiscard]] bool can_claim_level_upgrade(const std::string &level_id) const override;
    [[nodiscard]] bool can_claim_rescue_draft(const std::string &level_id) const override;
    [[nodiscard]] std::string get_frontier_level_id() const override;
    [[nodiscard]] int get_highest_level_score(const std::string &level_id) const override;
    [[nodiscard]] std::string get_current_level_id() const override { return current_level_id_; }
    [[nodiscard]] std::vector<ProgressionLevelUnlock> get_level_unlock_data() const override { return catalog_.get_progression_level_unlocks(); }
    [[nodiscard]] ProgressionUnitStats get_effective_friendly_unit_stats(const ProgressionUnitStats &base_stats) const override;
    [[nodiscard]] int get_effective_starting_energy(int base) const override;
    [[nodiscard]] int get_effective_energy_regen() const override;
    [[nodiscard]] float get_effective_bounty_multiplier() const override;
    [[nodiscard]] int get_effective_base_integrity(int base) const override;
    bool select_level(const std::string &level_id) override;
    [[nodiscard]] ProgressionMatchResult complete_level(const std::string &level_id, int level_score, bool victory) override;
    bool claim_upgrade(const ProgressionRewardClaim &claim) override;
    [[nodiscard]] std::vector<std::string> build_new_unlock_descriptions(const std::vector<std::string> &level_ids) const override;
    [[nodiscard]] ProgressionRewardViewModel build_reward_view_model(const ProgressionRewardDraft &draft) const override;
    [[nodiscard]] std::vector<ProgressionUpgradeCardViewModel> build_owned_upgrade_cards() const override;
    [[nodiscard]] ProgressionOverviewSnapshot build_progression_overview() const override;

    // Godot-facing wrappers and adapter conveniences
    [[nodiscard]] PackedStringArray get_unlocked_units_godot() const;
    [[nodiscard]] PackedStringArray get_unlocked_levels_godot() const;
    [[nodiscard]] PackedStringArray get_owned_upgrades_godot() const;
    [[nodiscard]] bool has_upgrade_godot(const String &upgrade_id) const;
    [[nodiscard]] bool is_level_completed_godot(const String &level_id) const;
    [[nodiscard]] bool is_level_unlocked_godot(const String &level_id) const;
    [[nodiscard]] bool can_claim_level_upgrade_godot(const String &level_id) const;
    [[nodiscard]] bool can_claim_rescue_draft_godot(const String &level_id) const;
    [[nodiscard]] String get_claimed_upgrade_for_level_godot(const String &level_id) const;
    [[nodiscard]] String get_frontier_level_id_godot() const;
    [[nodiscard]] int get_highest_level_score_godot(const String &level_id) const;
    [[nodiscard]] int get_rescue_drafts_claimed_godot(const String &level_id) const;
    [[nodiscard]] int get_next_rescue_draft_threshold_godot(const String &level_id) const;
    [[nodiscard]] String get_current_level_id_godot() const;
    void set_current_level_id_godot(const String &level_id);
    [[nodiscard]] UnitConfig get_effective_friendly_unit_config(const UnitConfig &base_config) const;
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_upgrade_draft_for_level_godot(const String &level_id);
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_rescue_draft_for_level_godot(const String &level_id);
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_owned_upgrade_cards_godot() const;
    int get_completed_level_count() const;

    // Progression queries for gameplay and presentation
    const std::vector<UpgradeCardDefinition> &get_upgrade_card_data() const { return upgrade_catalog_.get_cards(); }

    // Mutators
    void add_score(int amount);
    void mark_level_completed(const String &level_id, int level_score);
    bool claim_level_upgrade(const String &level_id, const String &upgrade_id);
    bool claim_rescue_draft(const String &level_id, const String &upgrade_id);
    void save();

  protected:
    static void _bind_methods();

  private:
    void load_save();
    void create_default_save();
    [[nodiscard]] int get_owned_upgrade_count(const std::string &upgrade_id) const;

    static CampaignService *singleton_;

    ProgressionCatalog catalog_;
    UpgradeCatalog upgrade_catalog_;
    UnitDataLoader unit_catalog_;
    ProgressionSaveRepository save_repository_;
    StdRandomSource random_;
    ProgressionCampaignUseCases use_cases_;
    PlayerProfile save_data_;

    // Transient
    std::string current_level_id_ = "level_01";
};

} // namespace defn

#endif
