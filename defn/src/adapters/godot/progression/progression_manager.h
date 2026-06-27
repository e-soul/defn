#ifndef PROGRESSION_MANAGER_H
#define PROGRESSION_MANAGER_H

#include "progression_catalog.h"
#include "progression_rules.h"
#include "progression_save_repository.h"
#include "progression_service.h"
#include "score_screen_models.h"
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
    CampaignService() = default;

    static CampaignService *get_singleton();
    static void register_singleton();
    static void unregister_singleton();

    void initialize();

    // Queries
    [[nodiscard]] int get_total_score() const override { return save_data_.total_score; }
    [[nodiscard]] PackedStringArray get_unlocked_units() const override;
    PackedStringArray get_unlocked_levels() const;
    PackedStringArray get_owned_upgrades() const;
    bool has_upgrade(const String &upgrade_id) const;
    [[nodiscard]] bool is_level_completed(const String &level_id) const override;
    [[nodiscard]] bool is_level_unlocked(const String &level_id) const override;
    [[nodiscard]] bool can_claim_level_upgrade(const String &level_id) const override;
    [[nodiscard]] bool can_claim_rescue_draft(const String &level_id) const override;
    String get_claimed_upgrade_for_level(const String &level_id) const;
    [[nodiscard]] String get_frontier_level_id() const override;
    [[nodiscard]] int get_highest_level_score(const String &level_id) const override;
    int get_rescue_drafts_claimed(const String &level_id) const;
    int get_next_rescue_draft_threshold(const String &level_id) const;
    [[nodiscard]] UnitConfig get_effective_friendly_unit_config(const UnitConfig &base_config) const override;
    [[nodiscard]] int get_effective_starting_energy(int base) const override;
    [[nodiscard]] int get_effective_energy_regen() const override;
    [[nodiscard]] real_t get_effective_bounty_multiplier() const override;
    [[nodiscard]] int get_effective_base_integrity(int base) const override;
    int get_completed_level_count() const;
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_upgrade_draft_for_level(const String &level_id) const override;
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_rescue_draft_for_level(const String &level_id) const override;
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_owned_upgrade_cards() const override;

    [[nodiscard]] String get_current_level_id() const override { return current_level_id_; }
    void set_current_level_id(const String &level_id) { current_level_id_ = level_id; }

    // Progression queries for gameplay and presentation
    [[nodiscard]] const std::vector<LevelUnlock> &get_level_unlock_data() const override { return catalog_.get_level_unlocks(); }
    const std::vector<UpgradeCardDefinition> &get_upgrade_card_data() const { return upgrade_catalog_.get_cards(); }

    // Mutators
    void add_score(int amount) override;
    void mark_level_completed(const String &level_id, int level_score) override;
    bool claim_level_upgrade(const String &level_id, const String &upgrade_id) override;
    bool claim_rescue_draft(const String &level_id, const String &upgrade_id) override;
    void save() override;

  protected:
    static void _bind_methods();

  private:
    static UpgradeCardViewModel build_upgrade_card_view(const UpgradeCardDefinition &card);

    std::vector<UpgradeCardViewModel> build_upgrade_draft() const;
    void load_save();
    void create_default_save();
    void grant_upgrade(const String &upgrade_id);
    int get_owned_upgrade_count(const String &upgrade_id) const;
    void set_rescue_drafts_claimed(const String &level_id, int claimed_count);

    static CampaignService *singleton_;

    ProgressionCatalog catalog_;
    UpgradeCatalog upgrade_catalog_;
    PlayerProfile save_data_;

    // Transient
    String current_level_id_ = "level_01";
};

} // namespace defn

#endif
