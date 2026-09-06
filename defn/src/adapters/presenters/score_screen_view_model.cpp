// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "score_screen_view_model.h"

#include <string>

namespace defn {

namespace {

const char *campaign_title(bool victory) { return victory ? "VICTORY" : "DEFEAT"; }

std::string format_ratio(int current, int total) { return std::to_string(current) + " / " + std::to_string(total); }

std::string with_record(const std::string &value, bool is_record) { return is_record ? value + "  BEST" : value; }

std::string format_best_run(int wave, int score) { return "WAVE " + std::to_string(wave) + "  /  " + std::to_string(score); }

ScoreScreenPresentationInput to_presentation_input(const ScoreScreenModel &model) {
    ScoreScreenPresentationInput input;
    input.victory = model.victory;
    input.enemies_killed = model.enemies_killed;
    input.kill_score = model.kill_score;
    input.hearts_remaining = model.hearts_remaining;
    input.hearts_total = model.hearts_total;
    input.integrity_bonus = model.integrity_bonus;
    input.survival_bonus = model.survival_bonus;
    input.completion_bonus = model.completion_bonus;
    input.level_score = model.level_score;
    input.new_total_score = model.new_total_score;
    input.endless = model.endless;
    input.next_level_id = model.next_level_id;
    input.reward_available = !model.reward.available_upgrades.empty();
    input.reward_requires_selection = model.reward.requires_selection();
    input.reward_title = model.reward.title;
    input.reward_subtitle = model.reward.subtitle;
    input.new_unlocks = model.new_unlocks;
    input.owned_upgrades_visible = !model.owned_upgrades.empty();
    return input;
}

} // namespace

ScoreScreenViewModel build_score_screen_view_model(const ScoreScreenPresentationInput &input) {
    const bool endless_run = input.endless.run;

    ScoreScreenViewModel view_model;
    // A run that was never going to be completed cannot be lost either. "RUN OVER" is the honest reading, and it is
    // also what stops the mode from feeling like a level the player keeps failing.
    view_model.title = campaign_title(input.victory);
    if (endless_run) {
        view_model.title = "RUN OVER";
    }
    view_model.victory = input.victory;

    if (endless_run) {
        view_model.stat_rows.emplace_back("Wave Reached:", with_record(std::to_string(input.endless.wave_reached), input.endless.record_wave));
    }
    view_model.stat_rows.emplace_back("Enemies Killed:", std::to_string(input.enemies_killed));
    view_model.stat_rows.emplace_back("Kill Score:", std::to_string(input.kill_score));
    view_model.stat_rows.emplace_back("Hearts Remaining:", format_ratio(input.hearts_remaining, input.hearts_total));
    view_model.stat_rows.emplace_back("Integrity Bonus:", std::to_string(input.integrity_bonus));
    if (input.survival_bonus > 0) {
        view_model.stat_rows.emplace_back("Survival Bonus:", std::to_string(input.survival_bonus));
    }
    if (input.victory) {
        view_model.stat_rows.emplace_back("Completion Bonus:", std::to_string(input.completion_bonus));
    }
    view_model.stat_rows.emplace_back(endless_run ? "Run Score:" : "Level Score:", with_record(std::to_string(input.level_score), input.endless.record_score));
    if (endless_run) {
        view_model.stat_rows.emplace_back("Best Run:", format_best_run(input.endless.best_wave, input.endless.best_score));
    }
    view_model.stat_rows.emplace_back("Career Total:", std::to_string(input.new_total_score));

    const bool actions_enabled = !input.reward_requires_selection;
    // A finished run has no next level and nothing to complete, so the forward action is a fresh run.
    view_model.next_level_button_visible = input.victory && !endless_run && !input.next_level_id.empty();
    view_model.next_level_button_enabled = actions_enabled;
    view_model.retry_button_visible = !endless_run;
    view_model.retry_button_enabled = actions_enabled;
    view_model.endless_button_visible = endless_run || input.endless.available;
    view_model.endless_button_enabled = actions_enabled;
    view_model.endless_button_label = endless_run ? "Retry" : "Endless";
    view_model.campaign_button_enabled = actions_enabled;
    view_model.reward_available = input.reward_available;
    view_model.reward_title = input.reward_title.empty() ? "CHOOSE 1 UPGRADE" : input.reward_title;
    view_model.reward_subtitle = input.reward_subtitle;
    view_model.new_unlocks = input.new_unlocks;
    if (input.endless.unlocked) {
        view_model.new_unlocks.emplace_back("NEW UNLOCK: ENDLESS MODE!");
    }
    view_model.owned_upgrades_visible = input.owned_upgrades_visible;
    return view_model;
}

ScoreScreenViewModel ScoreScreenPresenter::build(const ScoreScreenModel &model) { return build_score_screen_view_model(to_presentation_input(model)); }

} // namespace defn
