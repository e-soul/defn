#include "test_harness.h"

#include "progression_presentation.h"

namespace defn {

DEFN_TEST(progression_presentation_formats_reward_titles_and_subtitles) {
    DEFN_CHECK_EQ(ProgressionPresentation::format_level_name("level_02"), std::string("Level 02"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_title(ProgressionRewardSource::FIRST_CLEAR, "level_02"), std::string("FIRST CLEAR UPGRADE: Level 02"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_title(ProgressionRewardSource::RESCUE, "level_03"), std::string("RESCUE DRAFT: Level 03"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_title(ProgressionRewardSource::NONE, ""), std::string("CHOOSE 1 UPGRADE"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_subtitle(ProgressionRewardSource::FIRST_CLEAR, "level_02"),
                  std::string("Level 02 cleared for the first time."));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_subtitle(ProgressionRewardSource::RESCUE, "level_03"),
                  std::string("Career progress unlocked emergency support for Level 03."));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_subtitle(ProgressionRewardSource::NONE, ""), std::string());
}

DEFN_TEST(progression_presentation_describes_new_unlock_ids) {
    const std::vector<std::string> unlocks = ProgressionPresentation::describe_new_unlocks({"level_02", "challenge_room"});

    DEFN_REQUIRE(unlocks.size() == static_cast<size_t>(2));
    DEFN_CHECK_EQ(unlocks[0], std::string("NEW UNLOCK: Level 02!"));
    DEFN_CHECK_EQ(unlocks[1], std::string("NEW UNLOCK: Challenge Room!"));
}

DEFN_TEST(progression_presentation_formats_level_button_state) {
    DEFN_CHECK_EQ(ProgressionPresentation::format_level_button_label({.level_id = "level_01"}, true, true, 350), std::string("Level 01 - Best: 350"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_level_button_label({.level_id = "level_02", .requires_completed = "level_01"}, false, false, 0),
                  std::string("Level 02 (Complete Level 01)"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_level_button_label({.level_id = "challenge"}, false, false, 0), std::string("Challenge (Locked)"));
}

DEFN_TEST(progression_presentation_builds_upgrade_card_view_model) {
    const ProgressionUpgradeCardViewModel view_model = ProgressionPresentation::build_upgrade_card_view_model(
        {.id = "hp", .name = "Extra Plating", .description = "More health.", .emoji = "+", .category = "defense"}, 2);

    DEFN_CHECK_EQ(view_model.id, std::string("hp"));
    DEFN_CHECK_EQ(view_model.name, std::string("Extra Plating"));
    DEFN_CHECK_EQ(view_model.description, std::string("More health."));
    DEFN_CHECK_EQ(view_model.emoji, std::string("+"));
    DEFN_CHECK_EQ(view_model.category, std::string("defense"));
    DEFN_CHECK_EQ(view_model.owned_count, 2);
}

DEFN_TEST(progression_presentation_builds_reward_view_model_from_draft_ids) {
    const ProgressionRewardViewModel view_model = ProgressionPresentation::build_reward_view_model(
        {.source = ProgressionRewardSource::FIRST_CLEAR, .level_id = "level_01", .upgrade_ids = {"hp", "missing"}},
        {{.id = "hp", .name = "Extra Plating", .description = "More health.", .emoji = "+", .category = "defense"}});

    DEFN_CHECK_EQ(view_model.source, std::string("first_clear"));
    DEFN_CHECK_EQ(view_model.level_id, std::string("level_01"));
    DEFN_CHECK_EQ(view_model.title, std::string("FIRST CLEAR UPGRADE: Level 01"));
    DEFN_REQUIRE(view_model.available_upgrades.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(view_model.available_upgrades[0].id, std::string("hp"));
}

} // namespace defn