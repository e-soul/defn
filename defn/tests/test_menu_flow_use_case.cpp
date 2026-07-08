#include "test_harness.h"

#include "menu_flow_use_case.h"

namespace defn {

namespace {

class FakeProgressionService : public ProgressionService {
  public:
    bool allow_selection = true;
    std::string selected_level;

    [[nodiscard]] int get_total_score() const override { return 0; }
    [[nodiscard]] std::vector<std::string> get_unlocked_units() const override { return {}; }
    [[nodiscard]] bool is_level_completed(const std::string & /*level_id*/) const override { return false; }
    [[nodiscard]] bool is_level_unlocked(const std::string & /*level_id*/) const override { return true; }
    [[nodiscard]] bool can_claim_level_upgrade(const std::string & /*level_id*/) const override { return false; }
    [[nodiscard]] bool can_claim_rescue_draft(const std::string & /*level_id*/) const override { return false; }
    [[nodiscard]] std::string get_frontier_level_id() const override { return {}; }
    [[nodiscard]] int get_highest_level_score(const std::string & /*level_id*/) const override { return 0; }
    [[nodiscard]] std::string get_current_level_id() const override { return selected_level; }
    [[nodiscard]] std::vector<ProgressionLevelUnlock> get_level_unlock_data() const override { return {}; }
    [[nodiscard]] ProgressionUnitStats get_effective_friendly_unit_stats(const ProgressionUnitStats &base_stats) const override { return base_stats; }
    [[nodiscard]] int get_effective_starting_energy(int base) const override { return base; }
    [[nodiscard]] int get_effective_energy_regen() const override { return 0; }
    [[nodiscard]] float get_effective_bounty_multiplier() const override { return 1.0F; }
    [[nodiscard]] int get_effective_base_integrity(int base) const override { return base; }
    bool select_level(const std::string &level_id) override {
        if (!allow_selection) {
            return false;
        }
        selected_level = level_id;
        return true;
    }
    [[nodiscard]] ProgressionMatchResult complete_level(const std::string & /*level_id*/, int /*level_score*/, bool /*victory*/) override { return {}; }
    bool claim_upgrade(const ProgressionRewardClaim & /*claim*/) override { return false; }
    [[nodiscard]] std::vector<std::string> build_new_unlock_descriptions(const std::vector<std::string> & /*level_ids*/) const override { return {}; }
    [[nodiscard]] ProgressionRewardViewModel build_reward_view_model(const ProgressionRewardDraft & /*draft*/) const override { return {}; }
    [[nodiscard]] std::vector<ProgressionUpgradeCardViewModel> build_owned_upgrade_cards() const override { return {}; }
};

} // namespace

DEFN_TEST(menu_flow_use_case_maps_menu_intents_to_views_and_navigation) {
    MenuFlowUseCase flow;

    const MenuFlowResult goto_menu = flow.handle({.type = MenuIntentType::GotoMenu, .target = "options_menu"});
    DEFN_CHECK_EQ(goto_menu.view, MenuFlowView::Menu);
    DEFN_CHECK_EQ(goto_menu.menu_name, std::string("options_menu"));
    DEFN_CHECK(!goto_menu.navigation.has_value());

    DEFN_CHECK_EQ(flow.handle({.type = MenuIntentType::ShowLevelSelect}).view, MenuFlowView::LevelSelect);
    DEFN_CHECK_EQ(flow.handle({.type = MenuIntentType::ShowProgression}).view, MenuFlowView::Progression);

    const MenuFlowResult start_game = flow.handle({.type = MenuIntentType::StartGame});
    DEFN_REQUIRE(start_game.navigation.has_value());
    DEFN_CHECK_EQ(start_game.navigation->destination, SceneNavigationDestination::CurrentLevel);

    const MenuFlowResult quit = flow.handle({.type = MenuIntentType::Quit});
    DEFN_REQUIRE(quit.navigation.has_value());
    DEFN_CHECK_EQ(quit.navigation->destination, SceneNavigationDestination::Quit);

    DEFN_CHECK(flow.handle({.type = MenuIntentType::Resume}).resume);
}

DEFN_TEST(menu_flow_use_case_selects_level_before_requesting_level_navigation) {
    FakeProgressionService progression;
    MenuFlowUseCase flow(&progression);

    const MenuFlowResult selected = flow.select_level("level_02");
    DEFN_CHECK_EQ(progression.selected_level, std::string("level_02"));
    DEFN_REQUIRE(selected.navigation.has_value());
    DEFN_CHECK_EQ(selected.navigation->destination, SceneNavigationDestination::Level);
    DEFN_CHECK_EQ(selected.navigation->level_id, std::string("level_02"));

    progression.allow_selection = false;
    const MenuFlowResult rejected = flow.select_level("level_03");
    DEFN_CHECK(!rejected.navigation.has_value());
    DEFN_CHECK_EQ(progression.selected_level, std::string("level_02"));
}

DEFN_TEST(menu_flow_use_case_builds_main_menu_navigation_request) {
    const MenuFlowResult result = MenuFlowUseCase().request_main_menu();
    DEFN_REQUIRE(result.navigation.has_value());
    DEFN_CHECK_EQ(result.navigation->destination, SceneNavigationDestination::MainMenu);
}

} // namespace defn