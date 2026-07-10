#include "test_harness.h"

#include "deploy_card_view_model.h"
#include "hud_presenter.h"
#include "match_result_cutscene_view_model.h"
#include "menu_view_model.h"
#include "score_screen_view_model.h"

namespace defn {

DEFN_TEST(match_result_cutscene_view_model_builds_victory_interlude) {
    const MatchResultCutsceneModel view_model = MatchResultCutscenePresenter::build(true);

    DEFN_CHECK(view_model.victory);
    DEFN_CHECK_EQ(view_model.label, std::string("AREA SECURED"));
    DEFN_CHECK_CLOSE(view_model.label_color.r, 0.2F, 0.001F);
    DEFN_CHECK_CLOSE(view_model.label_color.g, 1.0F, 0.001F);
    DEFN_CHECK_CLOSE(view_model.label_color.b, 0.3F, 0.001F);
    DEFN_CHECK_EQ(view_model.primary_sfx_path, std::string("res://assets/sfx/area_secured_sting.wav"));
    DEFN_CHECK_EQ(view_model.base_destroyed_sfx_path, std::string());
    DEFN_CHECK_EQ(view_model.celebrant_side, MatchResultCelebrantSide::Friendly);
    DEFN_CHECK(!view_model.shake_base);
    DEFN_CHECK_CLOSE(view_model.duration_seconds, 4.0, 0.001);
    DEFN_CHECK_CLOSE(view_model.reveal_delay_seconds, 0.0, 0.001);
    DEFN_CHECK_EQ(view_model.pre_reveal_animation_name, std::string());
    DEFN_CHECK_EQ(view_model.reveal_animation_name, std::string("happy"));
}

DEFN_TEST(match_result_cutscene_view_model_builds_defeat_interlude) {
    const MatchResultCutsceneModel view_model = MatchResultCutscenePresenter::build(false);

    DEFN_CHECK(!view_model.victory);
    DEFN_CHECK_EQ(view_model.label, std::string("DEFEAT"));
    DEFN_CHECK_CLOSE(view_model.label_color.r, 1.0F, 0.001F);
    DEFN_CHECK_CLOSE(view_model.label_color.g, 0.2F, 0.001F);
    DEFN_CHECK_CLOSE(view_model.label_color.b, 0.2F, 0.001F);
    DEFN_CHECK_EQ(view_model.primary_sfx_path, std::string("res://assets/sfx/defeat_sting.wav"));
    DEFN_CHECK_EQ(view_model.base_destroyed_sfx_path, std::string("res://assets/sfx/base_destroyed_collapse.wav"));
    DEFN_CHECK_EQ(view_model.celebrant_side, MatchResultCelebrantSide::Hostile);
    DEFN_CHECK(view_model.shake_base);
    DEFN_CHECK_CLOSE(view_model.duration_seconds, 4.0, 0.001);
    DEFN_CHECK_CLOSE(view_model.reveal_delay_seconds, 1.5, 0.001);
    DEFN_CHECK_EQ(view_model.pre_reveal_animation_name, std::string("idle"));
    DEFN_CHECK_EQ(view_model.reveal_animation_name, std::string("happy"));
}

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

DEFN_TEST(hud_presenter_formats_match_state_and_deploy_card_affordability) {
    const HudModel model = HudPresenter::build({
        .energy = 42,
        .current_wave = 2,
        .total_waves = 5,
        .hearts = 3,
        .score = 125,
        .level_number = 4,
        .level_name = "Factory",
        .deploy_cards = {{.unit_id = "operator", .title = "Operator", .cost = 25}, {.unit_id = "tank", .title = "Tank", .cost = 75}},
    });

    DEFN_CHECK_EQ(model.energy_text, std::string("\u26A1 Energy: 42"));
    DEFN_CHECK_EQ(model.score_text, std::string("Score: 125"));
    DEFN_CHECK_EQ(model.wave_text, std::string("WAVE 2 / 5"));
    DEFN_CHECK_EQ(model.level_text, std::string("LEVEL 4 - Factory"));
    DEFN_CHECK(model.level_visible);
    DEFN_CHECK_EQ(model.visible_hearts, 3);
    DEFN_REQUIRE(model.deploy_cards.size() == static_cast<size_t>(2));
    DEFN_CHECK(model.deploy_cards[0].enabled);
    DEFN_CHECK(!model.deploy_cards[1].enabled);
}

DEFN_TEST(menu_view_model_maps_actions_to_typed_intents_and_disabled_entries) {
    const MenuScreenViewModel view_model = build_menu_screen_view_model({
        .name = "main_menu",
        .entries = {{.id = "play", .label = "Play", .intent_type = MenuIntentType::GotoMenu, .target = "game_menu"},
                    {.id = "disabled", .label = "Coming Soon"},
                    {.id = "quit", .label = "Quit", .intent_type = MenuIntentType::Quit}},
    });

    DEFN_REQUIRE(view_model.buttons.size() == static_cast<size_t>(3));
    DEFN_CHECK_EQ(view_model.buttons[0].intent.type, MenuIntentType::GotoMenu);
    DEFN_CHECK_EQ(view_model.buttons[0].intent.target, std::string("game_menu"));
    DEFN_CHECK(view_model.buttons[0].enabled);
    DEFN_CHECK_EQ(view_model.buttons[1].intent.type, MenuIntentType::None);
    DEFN_CHECK(!view_model.buttons[1].enabled);
    DEFN_CHECK_EQ(view_model.buttons[2].intent.type, MenuIntentType::Quit);
}

DEFN_TEST(menu_view_model_keeps_options_and_back_button_as_plain_data) {
    const MenuScreenViewModel view_model = build_menu_screen_view_model({
        .name = "options_menu",
        .type = MenuScreenType::Options,
        .settings = {{.id = "display_mode",
                      .label = "Display Mode",
                      .setting_id = "display_mode",
                      .kind = MenuSettingViewKind::DisplayMode,
                      .options = {{.label = "Windowed", .value = "0"}}}},
        .back = MenuActionPresentationInput{.label = "Back", .intent_type = MenuIntentType::GotoMenu, .target = "main_menu"},
    });

    DEFN_CHECK_EQ(view_model.type, MenuScreenType::Options);
    DEFN_REQUIRE(view_model.settings.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(view_model.settings[0].kind, MenuSettingViewKind::DisplayMode);
    DEFN_REQUIRE(view_model.back_button.has_value());
    DEFN_CHECK_EQ(view_model.back_button->intent.type, MenuIntentType::GotoMenu);
    DEFN_CHECK_EQ(view_model.back_button->intent.target, std::string("main_menu"));
}

DEFN_TEST(menu_view_model_builds_standard_level_and_progression_screens) {
    const LevelSelectViewModel level_select = build_level_select_view_model({{.level_id = "level_01", .label = "Level 01", .unlocked = true}});
    DEFN_CHECK_EQ(level_select.title, std::string("SELECT LEVEL"));
    DEFN_REQUIRE(level_select.levels.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(level_select.back_button.intent.type, MenuIntentType::GotoMenu);
    DEFN_CHECK_EQ(level_select.back_button.intent.target, std::string("game_menu"));

    const ProgressionScreenViewModel progression = build_progression_screen_view_model();
    DEFN_CHECK_EQ(progression.title, std::string("YOUR UPGRADES"));
    DEFN_CHECK_EQ(progression.back_button.intent.type, MenuIntentType::GotoMenu);
}

} // namespace defn