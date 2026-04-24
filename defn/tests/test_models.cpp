#include "test_harness.h"

#include "menu_models.h"
#include "score_screen_models.h"

namespace defn {

DEFN_TEST(menu_content_data_find_menu_returns_expected_definition) {
    MenuContentData data;
    data.menus.push_back({.name = "main_menu"});
    data.menus.push_back({.name = "pause_menu"});

    const MenuDefinition *menu = data.find_menu("pause_menu");
    DEFN_REQUIRE(menu != nullptr);
    DEFN_CHECK_EQ(menu->name, String("pause_menu"));
    DEFN_CHECK(data.find_menu("missing") == nullptr);
}

DEFN_TEST(score_screen_reward_find_upgrade_returns_expected_card) {
    ScoreScreenRewardModel reward;
    reward.available_upgrades.push_back({.id = "alpha", .name = "Alpha"});
    reward.available_upgrades.push_back({.id = "beta", .name = "Beta"});

    const UpgradeCardViewModel *upgrade = reward.find_upgrade("beta");
    DEFN_REQUIRE(upgrade != nullptr);
    DEFN_CHECK_EQ(upgrade->name, String("Beta"));
    DEFN_CHECK(reward.find_upgrade("gamma") == nullptr);
}

} // namespace defn