#ifndef MATCH_OUTPUTS_H
#define MATCH_OUTPUTS_H

#include "match_summary.h"
#include "unit_runtime_config_resolver.h"
#include "unit_runtime_profile.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

enum class MatchUnitSide { Friendly, Hostile };

struct MatchPosition {
    double x = 0.0;
    double y = 0.0;
};

struct SpawnUnitIntent {
    std::string unit_id;
    MatchUnitSide side = MatchUnitSide::Hostile;
    MatchPosition position;
    UnitRuntimeProfile runtime_profile;
    ResolvedUnitRuntimeConfig resolved_runtime_config;
};

struct ResourceChanged {
    int energy = 0;
};

struct IntegrityChanged {
    int hearts = 0;
};

struct WaveChanged {
    int current_wave = 0;
    int total_waves = 0;
};

struct ScoreChanged {
    int kill_score = 0;
    int total_score = 0;
    int bounty_awarded = 0;
};

struct MatchUpgradeOption {
    std::string id;
    std::string name;
    std::string description;
    std::string emoji;
    std::string category;
    int owned_count = 0;
};

struct MatchRewardOptions {
    std::string source;
    std::string level_id;
    std::string title;
    std::string subtitle;
    std::vector<MatchUpgradeOption> available_upgrades;
    std::optional<MatchUpgradeOption> selected_upgrade;

    [[nodiscard]] bool requires_selection() const { return !available_upgrades.empty() && !selected_upgrade.has_value(); }

    [[nodiscard]] const MatchUpgradeOption *find_upgrade(const std::string &upgrade_id) const {
        for (const auto &upgrade : available_upgrades) {
            if (upgrade.id == upgrade_id) {
                return &upgrade;
            }
        }

        return nullptr;
    }
};

struct MatchEnded {
    bool victory = false;
    MatchSummaryModel summary_model;
    MatchRewardOptions reward_options;
    std::vector<MatchUpgradeOption> owned_upgrades;
};

struct MatchUpdate {
    std::vector<SpawnUnitIntent> spawn_unit_intents;
    std::optional<ResourceChanged> resource_changed;
    std::optional<IntegrityChanged> integrity_changed;
    std::optional<WaveChanged> wave_changed;
    std::optional<ScoreChanged> score_changed;
    std::optional<MatchEnded> match_ended;
};

} // namespace defn

#endif