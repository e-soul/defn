// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef PROGRESSION_MODELS_H
#define PROGRESSION_MODELS_H

#include <string>
#include <vector>

namespace defn {

// Which kind of match the player is about to start. An explicit mode rather than a reserved level id: the id is a
// content key everywhere else in the project, and overloading it would put a magic string into path resolution.
enum class MatchMode { CAMPAIGN, ENDLESS };

// The endless mode is recorded per threat level, so a later ascension ladder needs no second save migration. There
// is one threat level today.
inline constexpr int DEFAULT_ENDLESS_THREAT_LEVEL = 0;

struct EndlessRecord {
    int best_wave = 0;
    int best_score = 0;
};

// What one finished run did to the record, so the run-over screen can say which half of it was a personal best.
struct EndlessRunRecordResult {
    int wave_reached = 0;
    int score = 0;
    bool record_wave = false;
    bool record_score = false;
    EndlessRecord record;
};

enum class ProgressionRewardSource {
    NONE,
    FIRST_CLEAR,
    RESCUE,
};

inline std::string to_progression_reward_source_id(ProgressionRewardSource source) {
    switch (source) {
    case ProgressionRewardSource::FIRST_CLEAR:
        return "first_clear";
    case ProgressionRewardSource::RESCUE:
        return "rescue";
    case ProgressionRewardSource::NONE:
        return {};
    }

    return {};
}

inline ProgressionRewardSource progression_reward_source_from_id(const std::string &source_id) {
    if (source_id == "first_clear") {
        return ProgressionRewardSource::FIRST_CLEAR;
    }
    if (source_id == "rescue") {
        return ProgressionRewardSource::RESCUE;
    }
    return ProgressionRewardSource::NONE;
}

struct ProgressionUpgradePresentation {
    std::string id;
    std::string name;
    std::string description;
    std::string icon;
    std::string category;
};

struct ProgressionUpgradeCardViewModel {
    std::string id;
    std::string name;
    std::string description;
    std::string icon;
    std::string category;
    int owned_count = 0;
};

struct ProgressionRewardDraft {
    ProgressionRewardSource source = ProgressionRewardSource::NONE;
    std::string level_id;
    std::vector<std::string> upgrade_ids;

    [[nodiscard]] bool has_reward() const { return source != ProgressionRewardSource::NONE && !level_id.empty() && !upgrade_ids.empty(); }
};

struct ProgressionRewardClaim {
    ProgressionRewardSource source = ProgressionRewardSource::NONE;
    std::string level_id;
    std::string upgrade_id;
};

struct ProgressionRewardViewModel {
    std::string source;
    std::string level_id;
    std::string title;
    std::string subtitle;
    std::vector<ProgressionUpgradeCardViewModel> available_upgrades;
};

struct ProgressionMatchResult {
    int new_total_score = 0;
    // Edge-triggered on the run that first clears the level endless is gated behind, so replaying it announces
    // nothing. Read by the score screen; the map beacon reads availability instead.
    bool endless_unlocked = false;
    std::vector<std::string> new_unlock_level_ids;
    std::string next_level_id;
    ProgressionRewardDraft reward_draft;
};

enum class ProgressionEntityKind {
    BASE,
    UNIT,
    OPERATIONS,
};

struct ProgressionStatValue {
    std::string id;
    double base_value = 0.0;
    double effective_value = 0.0;
    double contribution = 0.0;
    bool contribution_only = false;
};

struct ProgressionUpgradeSource {
    ProgressionUpgradePresentation presentation;
    int owned_count = 0;
};

struct ProgressionEntitySnapshot {
    std::string id;
    ProgressionEntityKind kind = ProgressionEntityKind::UNIT;
    bool unlocked = false;
    std::string description;
    std::string portrait_path_template;
    std::string unlock_upgrade_name;
    std::vector<ProgressionStatValue> stats;
    std::vector<ProgressionUpgradeSource> contributing_upgrades;
};

struct ProgressionOverviewSnapshot {
    std::vector<ProgressionEntitySnapshot> entities;
    // Absent until the mode is unlocked. The roster screen is where a career is read, and a standing engagement is
    // part of a career even though it belongs to no unit.
    bool endless_available = false;
    EndlessRecord endless_record;
};

} // namespace defn

#endif
