// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SCORE_SCREEN_VIEW_MODEL_H
#define SCORE_SCREEN_VIEW_MODEL_H

#include "score_screen_models.h"

#include <string>
#include <utility>
#include <vector>

namespace defn {

struct ScoreScreenPresentationInput {
    bool victory = false;
    int enemies_killed = 0;
    int kill_score = 0;
    int hearts_remaining = 0;
    int hearts_total = 0;
    int integrity_bonus = 0;
    int survival_bonus = 0;
    int completion_bonus = 0;
    int level_score = 0;
    int new_total_score = 0;
    ScoreScreenEndlessModel endless;
    std::string next_level_id;
    bool reward_available = false;
    bool reward_requires_selection = false;
    std::string reward_title;
    std::string reward_subtitle;
    std::vector<std::string> new_unlocks;
    bool owned_upgrades_visible = false;
};

struct ScoreScreenViewModel {
    std::string title;
    bool victory = false;
    std::vector<std::pair<std::string, std::string>> stat_rows;
    bool next_level_button_visible = false;
    bool next_level_button_enabled = true;
    bool retry_button_visible = true;
    bool retry_button_enabled = true;
    // The one endless action. On a campaign score screen it reads "Endless" and starts the mode; on a run-over
    // screen it reads "Retry" and starts a fresh run, which is the same call either way.
    bool endless_button_visible = false;
    bool endless_button_enabled = true;
    std::string endless_button_label;
    bool campaign_button_enabled = true;
    bool reward_available = false;
    std::string reward_title;
    std::string reward_subtitle;
    std::vector<std::string> new_unlocks;
    bool owned_upgrades_visible = false;
};

[[nodiscard]] ScoreScreenViewModel build_score_screen_view_model(const ScoreScreenPresentationInput &input);

class ScoreScreenPresenter {
  public:
    ScoreScreenPresenter() = delete;

    [[nodiscard]] static ScoreScreenViewModel build(const ScoreScreenModel &model);
};

} // namespace defn

#endif
