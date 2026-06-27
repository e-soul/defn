#include "test_harness.h"

#include "deploy_card_view_model.h"
#include "score_screen_view_model.h"

namespace defn {

DEFN_TEST(score_screen_view_model_formats_victory_stats_and_buttons) {
    const ScoreScreenViewModel view_model = build_score_screen_view_model({
        .victory = true,
        .enemies_killed = 8,
        .kill_score = 120,
        .hearts_remaining = 2,
        .hearts_total = 3,
        .integrity_bonus = 50,
        .completion_bonus = 100,
        .level_score = 270,
        .new_total_score = 900,
        .next_level_id = "level_02",
    });

    DEFN_CHECK_EQ(view_model.title, std::string("VICTORY"));
    DEFN_CHECK_EQ(view_model.stat_rows.size(), static_cast<size_t>(7));
    DEFN_CHECK_EQ(view_model.stat_rows[2].second, std::string("2 / 3"));
    DEFN_CHECK(view_model.next_level_button_visible);
    DEFN_CHECK(view_model.next_level_button_enabled);
}

DEFN_TEST(score_screen_view_model_blocks_actions_until_reward_selected) {
    const ScoreScreenViewModel view_model = build_score_screen_view_model({
        .victory = false,
        .reward_available = true,
        .reward_requires_selection = true,
    });

    DEFN_CHECK_EQ(view_model.title, std::string("DEFEAT"));
    DEFN_CHECK(!view_model.next_level_button_visible);
    DEFN_CHECK(!view_model.retry_button_enabled);
    DEFN_CHECK(!view_model.main_menu_button_enabled);
    DEFN_CHECK_EQ(view_model.reward_title, std::string("CHOOSE 1 UPGRADE"));
}

DEFN_TEST(deploy_card_view_model_formats_title_cost_and_portrait) {
    const DeployCardViewModel view_model = build_deploy_card_view_model({
        .unit_id = "operator",
        .title = "Operator",
        .cost = 25,
        .animation_path_templates = {{"walk", "res://walk_%d.png"}, {"shoot", "res://shoot_%d.png"}},
    });

    DEFN_CHECK_EQ(view_model.unit_id, std::string("operator"));
    DEFN_CHECK_EQ(view_model.title, std::string("Operator"));
    DEFN_CHECK_EQ(view_model.cost, 25);
    DEFN_CHECK_EQ(view_model.portrait_path, std::string("res://shoot_0.png"));
}

DEFN_TEST(deploy_card_view_model_preserves_zero_padded_frame_templates) {
    const DeployCardViewModel view_model = build_deploy_card_view_model({
        .unit_id = "rifleman",
        .animation_path_templates = {{"shoot", "res://Shoot__%03d.png"}},
    });

    DEFN_CHECK_EQ(view_model.portrait_path, std::string("res://Shoot__000.png"));
}

} // namespace defn