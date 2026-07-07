#ifndef PROGRESSION_PORTS_H
#define PROGRESSION_PORTS_H

#include "player_profile.h"
#include "progression_models.h"
#include "progression_rules.h"
#include "upgrade_draft_builder.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

class ProfileRepository {
  public:
    virtual ~ProfileRepository() = default;

    [[nodiscard]] virtual std::optional<PlayerProfile> load_profile() const = 0;
    virtual bool save_profile(const PlayerProfile &profile) const = 0;
};

class ProgressionCatalogPort {
  public:
    virtual ~ProgressionCatalogPort() = default;

    [[nodiscard]] virtual std::vector<ProgressionLevelUnlock> get_progression_level_unlocks() const = 0;
};

class UpgradeCatalogPort {
  public:
    virtual ~UpgradeCatalogPort() = default;

    [[nodiscard]] virtual std::vector<std::string> get_base_unit_ids() const = 0;
    [[nodiscard]] virtual std::vector<ProgressionUpgradeCard> get_progression_upgrade_cards() const = 0;
    [[nodiscard]] virtual std::vector<UpgradeDraftCard> get_upgrade_draft_cards() const = 0;
    [[nodiscard]] virtual UpgradeDraftConfig get_upgrade_draft_config() const = 0;
    [[nodiscard]] virtual std::optional<ProgressionUpgradePresentation> find_upgrade_presentation(const std::string &upgrade_id) const = 0;
    [[nodiscard]] virtual std::vector<ProgressionUpgradePresentation> get_upgrade_presentations() const = 0;
};

} // namespace defn

#endif