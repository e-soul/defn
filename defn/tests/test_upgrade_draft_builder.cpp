#include "test_harness.h"

#include "scripted_random_source.h"
#include "upgrade_draft_builder.h"

namespace defn {

DEFN_TEST(upgrade_draft_builder_uses_reserved_unit_slot_and_weighted_rolls) {
    ProgressionProfile profile;
    profile.completed_levels.insert("level_01");
    profile.owned_upgrade_counts["starter"] = 1;

    const std::vector<UpgradeDraftCard> cards{
        {.id = "unit", .weight = 1, .grants_unit_unlock = true},
        {.id = "general_a", .weight = 1},
        {.id = "general_b", .weight = 5},
        {.id = "locked", .minimum_completed_levels = 2},
        {.id = "needs_starter", .weight = 1, .prerequisites = {"starter"}},
    };
    tests::ScriptedRandomSource random;
    random.push_int(1);
    random.push_int(2);
    random.push_int(1);

    const std::vector<std::string> draft = build_upgrade_draft(profile, cards, {.draft_size = 3, .reserve_unit_slot = true}, random);

    DEFN_CHECK_EQ(draft.size(), static_cast<size_t>(3));
    DEFN_CHECK_EQ(draft[0], std::string("unit"));
    DEFN_CHECK_EQ(draft[1], std::string("general_b"));
    DEFN_CHECK_EQ(draft[2], std::string("general_a"));
}

DEFN_TEST(upgrade_draft_builder_filters_max_picks_and_missing_prerequisites) {
    ProgressionProfile profile;
    profile.owned_upgrade_counts["owned"] = 1;

    const std::vector<UpgradeDraftCard> cards{
        {.id = "owned", .max_picks = 1},
        {.id = "needs_missing", .prerequisites = {"missing"}},
        {.id = "available", .weight = 1},
    };
    tests::ScriptedRandomSource random;
    random.push_int(1);

    const std::vector<std::string> draft = build_upgrade_draft(profile, cards, {.draft_size = 3, .reserve_unit_slot = false}, random);

    DEFN_CHECK_EQ(draft.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(draft[0], std::string("available"));
}

} // namespace defn