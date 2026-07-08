#include "score_screen_view_model.h"

#include <string>

namespace defn {

namespace {

std::string format_ratio(int current, int total) { return std::to_string(current) + " / " + std::to_string(total); }

ScoreScreenPresentationInput to_presentation_input(const ScoreScreenModel &model) {
    ScoreScreenPresentationInput input;
    input.victory = model.victory;
    input.enemies_killed = model.enemies_killed;
    input.kill_score = model.kill_score;
    input.hearts_remaining = model.hearts_remaining;
    input.hearts_total = model.hearts_total;
    input.integrity_bonus = model.integrity_bonus;
    input.completion_bonus = model.completion_bonus;
    input.level_score = model.level_score;
    input.new_total_score = model.new_total_score;
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
    ScoreScreenViewModel view_model;
    view_model.title = input.victory ? "VICTORY" : "DEFEAT";
    view_model.victory = input.victory;
    view_model.stat_rows.emplace_back("Enemies Killed:", std::to_string(input.enemies_killed));
    view_model.stat_rows.emplace_back("Kill Score:", std::to_string(input.kill_score));
    view_model.stat_rows.emplace_back("Hearts Remaining:", format_ratio(input.hearts_remaining, input.hearts_total));
    view_model.stat_rows.emplace_back("Integrity Bonus:", std::to_string(input.integrity_bonus));
    if (input.victory) {
        view_model.stat_rows.emplace_back("Completion Bonus:", std::to_string(input.completion_bonus));
    }
    view_model.stat_rows.emplace_back("Level Score:", std::to_string(input.level_score));
    view_model.stat_rows.emplace_back("Career Total:", std::to_string(input.new_total_score));

    const bool actions_enabled = !input.reward_requires_selection;
    view_model.next_level_button_visible = input.victory && !input.next_level_id.empty();
    view_model.next_level_button_enabled = actions_enabled;
    view_model.retry_button_enabled = actions_enabled;
    view_model.main_menu_button_enabled = actions_enabled;
    view_model.reward_available = input.reward_available;
    view_model.reward_title = input.reward_title.empty() ? "CHOOSE 1 UPGRADE" : input.reward_title;
    view_model.reward_subtitle = input.reward_subtitle;
    view_model.new_unlocks = input.new_unlocks;
    view_model.owned_upgrades_visible = input.owned_upgrades_visible;
    return view_model;
}

ScoreScreenViewModel ScoreScreenPresenter::build(const ScoreScreenModel &model) { return build_score_screen_view_model(to_presentation_input(model)); }

} // namespace defn