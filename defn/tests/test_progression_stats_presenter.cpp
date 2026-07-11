#include "test_harness.h"

#include "progression_stats_presenter.h"

namespace defn {

namespace {

ProgressionOverviewSnapshot sample_snapshot() {
    return {.entities = {{.id = "base", .kind = ProgressionEntityKind::BASE, .unlocked = true},
                         {.id = "breacher",
                          .kind = ProgressionEntityKind::UNIT,
                          .unlocked = true,
                          .description = "Frontline anchor.",
                          .portrait_path_template = "res://breacher_%03d.png",
                          .stats = {{.id = "health", .base_value = 400.0, .effective_value = 450.0},
                                    {.id = "firepower", .base_value = 8.0, .effective_value = 8.0},
                                    {.id = "mobility", .base_value = 58.0, .effective_value = 66.0},
                                    {.id = "deploy_cost", .base_value = 20.0, .effective_value = 20.0},
                                    {.id = "ignored", .effective_value = 99.0}},
                          .contributing_upgrades = {{.presentation = {.id = "plating", .name = "Plating", .description = "More health.", .emoji = "+"},
                                                     .owned_count = 2}}},
                         {.id = "marksman", .kind = ProgressionEntityKind::UNIT, .unlocked = false, .unlock_upgrade_name = "Sharpshooter Contract"},
                         {.id = "operations",
                          .kind = ProgressionEntityKind::OPERATIONS,
                          .unlocked = true,
                          .stats = {{.id = "starting_energy_bonus", .contribution = 35.0, .contribution_only = true},
                                    {.id = "energy_generation", .base_value = 1.0, .effective_value = 2.0},
                                    {.id = "bounty_bonus", .contribution = 0.5, .contribution_only = true}}}}};
}

} // namespace

DEFN_TEST(progression_stats_presenter_defaults_to_first_unlocked_deployable_and_formats_stats) {
    const auto model = ProgressionStatsPresenter::present(sample_snapshot(), "");

    DEFN_CHECK_EQ(model.selected_entity_id, std::string("breacher"));
    DEFN_CHECK_EQ(model.title, std::string("Breacher"));
    DEFN_REQUIRE(model.stats.size() == static_cast<size_t>(4));
    DEFN_CHECK_EQ(model.stats[0].value, std::string("450"));
    DEFN_CHECK_EQ(model.stats[0].detail, std::string("400 +50"));
    DEFN_CHECK_EQ(model.stats[1].detail, std::string());
    DEFN_REQUIRE(model.upgrades.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(model.upgrades[0].label, std::string("Plating x2"));
    DEFN_CHECK_EQ(model.selectors[2].locked_message, std::string("Unlock with Sharpshooter Contract"));
}

DEFN_TEST(progression_stats_presenter_formats_campaign_contributions_without_totals) {
    const auto model = ProgressionStatsPresenter::present(sample_snapshot(), "operations");

    DEFN_REQUIRE(model.stats.size() == static_cast<size_t>(3));
    DEFN_CHECK_EQ(model.stats[0].value, std::string("+35"));
    DEFN_CHECK_EQ(model.stats[0].detail, std::string());
    DEFN_CHECK_EQ(model.stats[1].value, std::string("2/s"));
    DEFN_CHECK_EQ(model.stats[1].detail, std::string("1 +1"));
    DEFN_CHECK_EQ(model.stats[2].value, std::string("+50%"));
}

DEFN_TEST(progression_stats_presenter_rejects_locked_or_unknown_selection) {
    DEFN_CHECK_EQ(ProgressionStatsPresenter::present(sample_snapshot(), "marksman").selected_entity_id, std::string("breacher"));
    DEFN_CHECK_EQ(ProgressionStatsPresenter::present(sample_snapshot(), "missing").selected_entity_id, std::string("breacher"));
}

} // namespace defn
