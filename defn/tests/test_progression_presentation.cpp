#include "test_harness.h"

#include "progression_presentation.h"

#include <algorithm>

namespace defn {

namespace {

bool contains_string(const std::vector<String> &values, const String &candidate) {
    return std::ranges::any_of(values, [&candidate](const String &value) { return value == candidate; });
}

class FakeProgressionService final : public ProgressionService {
  public:
    std::vector<LevelUnlock> level_unlocks;
    std::vector<String> unlocked_levels;
    std::vector<String> completed_levels;
    std::vector<std::pair<String, int>> best_scores;

    [[nodiscard]] int get_total_score() const override { return 0; }
    [[nodiscard]] PackedStringArray get_unlocked_units() const override { return {}; }
    [[nodiscard]] bool is_level_completed(const String &level_id) const override { return contains_string(completed_levels, level_id); }
    [[nodiscard]] bool is_level_unlocked(const String &level_id) const override { return contains_string(unlocked_levels, level_id); }
    [[nodiscard]] bool can_claim_level_upgrade(const String & /*level_id*/) const override { return false; }
    [[nodiscard]] bool can_claim_rescue_draft(const String & /*level_id*/) const override { return false; }
    [[nodiscard]] String get_frontier_level_id() const override { return {}; }
    [[nodiscard]] int get_highest_level_score(const String &level_id) const override {
        for (const auto &[candidate, score] : best_scores) {
            if (candidate == level_id) {
                return score;
            }
        }
        return 0;
    }
    [[nodiscard]] String get_current_level_id() const override { return {}; }
    [[nodiscard]] const std::vector<LevelUnlock> &get_level_unlock_data() const override { return level_unlocks; }
    [[nodiscard]] UnitConfig get_effective_friendly_unit_config(const UnitConfig &base_config) const override { return base_config; }
    [[nodiscard]] int get_effective_starting_energy(int base) const override { return base; }
    [[nodiscard]] int get_effective_energy_regen() const override { return 0; }
    [[nodiscard]] real_t get_effective_bounty_multiplier() const override { return 1.0F; }
    [[nodiscard]] int get_effective_base_integrity(int base) const override { return base; }
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_upgrade_draft_for_level(const String & /*level_id*/) const override { return {}; }
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_rescue_draft_for_level(const String & /*level_id*/) const override { return {}; }
    [[nodiscard]] std::vector<UpgradeCardViewModel> build_owned_upgrade_cards() const override { return {}; }
    void add_score(int /*amount*/) override {}
    void mark_level_completed(const String & /*level_id*/, int /*level_score*/) override {}
    bool claim_level_upgrade(const String & /*level_id*/, const String & /*upgrade_id*/) override { return false; }
    bool claim_rescue_draft(const String & /*level_id*/, const String & /*upgrade_id*/) override { return false; }
    void save() override {}
};

} // namespace

DEFN_TEST(progression_presentation_formats_reward_titles_and_subtitles) {
    DEFN_CHECK_EQ(ProgressionPresentation::format_level_name("level_02"), String("Level 02"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_title("first_clear", "level_02"), String("FIRST CLEAR UPGRADE: Level 02"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_title("rescue", "level_03"), String("RESCUE DRAFT: Level 03"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_title("", ""), String("CHOOSE 1 UPGRADE"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_subtitle("first_clear", "level_02"), String("Level 02 cleared for the first time."));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_subtitle("rescue", "level_03"), String("Career progress unlocked emergency support for Level 03."));
    DEFN_CHECK_EQ(ProgressionPresentation::format_reward_subtitle("", ""), String());
}

DEFN_TEST(progression_presentation_describes_new_unlocks_only_after_victory) {
    FakeProgressionService progression;
    progression.level_unlocks = {{.level_id = "level_02", .requires_completed = "level_01"}, {.level_id = "level_03", .requires_completed = "level_02"}};
    progression.unlocked_levels = {"level_02"};

    DEFN_CHECK_EQ(ProgressionPresentation::describe_new_unlocks(progression, false, "level_01").size(), 0);
    DEFN_CHECK_EQ(ProgressionPresentation::describe_new_unlocks(progression, true, "").size(), 0);

    const PackedStringArray unlocks = ProgressionPresentation::describe_new_unlocks(progression, true, "level_01");
    DEFN_REQUIRE(unlocks.size() == 1);
    DEFN_CHECK_EQ(unlocks[0], String("NEW UNLOCK: Level 02!"));
}

DEFN_TEST(progression_presentation_formats_level_button_state) {
    FakeProgressionService progression;
    progression.unlocked_levels = {"level_01"};
    progression.completed_levels = {"level_01"};
    progression.best_scores = {{"level_01", 350}};

    DEFN_CHECK_EQ(ProgressionPresentation::format_level_button_label(progression, {.level_id = "level_01"}), String("Level 01 - Best: 350"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_level_button_label(progression, {.level_id = "level_02", .requires_completed = "level_01"}),
                  String("Level 02 (Complete Level 01)"));
    DEFN_CHECK_EQ(ProgressionPresentation::format_level_button_label(progression, {.level_id = "challenge"}), String("Challenge (Locked)"));
}

} // namespace defn