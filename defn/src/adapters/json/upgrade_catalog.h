#ifndef UPGRADE_CATALOG_H
#define UPGRADE_CATALOG_H

#include "progression_ports.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <vector>

namespace defn {

using namespace godot;

enum class UpgradeEffectType {
    STARTING_ENERGY_DELTA,
    ENERGY_REGEN_DELTA,
    BOUNTY_MULTIPLIER_DELTA,
    BASE_INTEGRITY_DELTA,
    UNIT_HP_DELTA,
    UNIT_RANGED_DAMAGE_DELTA,
    UNIT_MOVE_SPEED_DELTA,
    UNIT_UNLOCK,
};

struct UpgradeCardEffect {
    UpgradeEffectType type = UpgradeEffectType::STARTING_ENERGY_DELTA;
    real_t value = 0.0F;
    String unit_id;
};

struct UpgradeCardDefinition {
    String id;
    String name;
    String description;
    String emoji;
    String category;
    int minimum_completed_levels = 0;
    int weight = 1;
    int max_picks = 1;
    std::vector<String> prerequisites;
    std::vector<UpgradeCardEffect> effects;

    bool grants_unit_unlock() const;
};

bool try_parse_upgrade_effect_type(const String &value, UpgradeEffectType &out_type);

class UpgradeCatalog : public UpgradeCatalogPort {
  public:
    bool load(const String &path);
    bool load_from_data(const Dictionary &data);

    const std::vector<String> &get_base_units() const { return base_units_; }
    const std::vector<UpgradeCardDefinition> &get_cards() const { return cards_; }
    int get_draft_size() const { return draft_size_; }
    bool should_reserve_unit_slot() const { return reserve_unit_slot_; }
    const UpgradeCardDefinition *find_card(const String &card_id) const;
    [[nodiscard]] std::vector<std::string> get_base_unit_ids() const override;
    [[nodiscard]] std::vector<ProgressionUpgradeCard> get_progression_upgrade_cards() const override;
    [[nodiscard]] std::vector<UpgradeDraftCard> get_upgrade_draft_cards() const override;
    [[nodiscard]] UpgradeDraftConfig get_upgrade_draft_config() const override;
    [[nodiscard]] std::optional<ProgressionUpgradePresentation> find_upgrade_presentation(const std::string &upgrade_id) const override;
    [[nodiscard]] std::vector<ProgressionUpgradePresentation> get_upgrade_presentations() const override;

  private:
    int draft_size_ = 3;
    bool reserve_unit_slot_ = true;
    std::vector<String> base_units_;
    std::vector<UpgradeCardDefinition> cards_;
};

} // namespace defn

#endif