#include "test_harness.h"

#include "progression_use_cases.h"
#include "scripted_random_source.h"

namespace defn {

namespace {

ProgressionUpgradeCard make_card(const std::string &id, int max_picks = 1) {
    return {.id = id, .max_picks = max_picks};
}

} // namespace

DEFN_TEST(progression_use_cases_complete_level_and_preserve_best_score) {
    ProgressionProfile profile;
    ProgressionUseCases::add_score(profile, 150);
    ProgressionUseCases::mark_level_completed(profile, "level_01", 200);
    ProgressionUseCases::mark_level_completed(profile, "level_01", 175);

    DEFN_CHECK_EQ(profile.total_score, 150);
    DEFN_CHECK(profile.completed_levels.contains("level_01"));
    DEFN_CHECK_EQ(profile.best_level_scores["level_01"], 200);
}

DEFN_TEST(progression_use_cases_claim_level_upgrade_once_and_enforce_max_picks) {
    ProgressionProfile profile;
    const std::vector<ProgressionUpgradeCard> cards{make_card("hp", 1), make_card("speed", 2)};

    DEFN_CHECK(ProgressionUseCases::claim_level_upgrade(profile, cards, "level_01", "hp"));
    DEFN_CHECK_EQ(profile.claimed_level_upgrades["level_01"], std::string("hp"));
    DEFN_CHECK_EQ(profile.owned_upgrade_counts["hp"], 1);
    DEFN_CHECK(!ProgressionUseCases::claim_level_upgrade(profile, cards, "level_01", "speed"));
    DEFN_CHECK(!ProgressionUseCases::claim_level_upgrade(profile, cards, "level_02", "hp"));
}

DEFN_TEST(progression_use_cases_claim_rescue_draft_for_frontier_threshold) {
    ProgressionProfile profile;
    profile.completed_levels.insert("level_01");
    profile.total_score = 250;
    const std::vector<ProgressionLevelUnlock> levels{{.level_id = "level_01"}, {.level_id = "level_02", .requires_completed = "level_01", .rescue_thresholds = {100, 250}}};
    const std::vector<ProgressionUpgradeCard> cards{make_card("rescue", 2)};

    DEFN_CHECK(ProgressionUseCases::claim_rescue_draft(profile, levels, cards, "level_02", "rescue"));
    DEFN_CHECK_EQ(profile.claimed_rescue_drafts["level_02"], 1);
    DEFN_CHECK_EQ(profile.owned_upgrade_counts["rescue"], 1);
    DEFN_CHECK(ProgressionUseCases::claim_rescue_draft(profile, levels, cards, "level_02", "rescue"));
    DEFN_CHECK_EQ(profile.claimed_rescue_drafts["level_02"], 2);
    DEFN_CHECK_EQ(profile.owned_upgrade_counts["rescue"], 2);
    DEFN_CHECK(!ProgressionUseCases::claim_rescue_draft(profile, levels, cards, "level_02", "rescue"));
}

DEFN_TEST(progression_use_cases_delegate_draft_selection_to_deterministic_random_source) {
    ProgressionProfile profile;
    const std::vector<UpgradeDraftCard> cards{{.id = "a", .weight = 1}, {.id = "b", .weight = 1}};
    tests::ScriptedRandomSource random;
    random.push_int(2);

    const std::vector<std::string> draft = ProgressionUseCases::build_upgrade_draft(profile, cards, {.draft_size = 1, .reserve_unit_slot = false}, random);

    DEFN_REQUIRE(draft.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(draft[0], std::string("b"));
}

} // namespace defn