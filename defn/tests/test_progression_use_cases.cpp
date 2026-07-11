#include "test_harness.h"

#include "progression_use_cases.h"
#include "scripted_random_source.h"
#include "unit_definition.h"

#include <optional>

namespace defn {

namespace {

ProgressionUpgradeCard make_card(const std::string &id, int max_picks = 1) { return {.id = id, .max_picks = max_picks}; }

class FakeProfileRepository final : public ProfileRepository {
  public:
    mutable std::optional<PlayerProfile> stored_profile;
    mutable int save_count = 0;

    [[nodiscard]] std::optional<PlayerProfile> load_profile() const override { return stored_profile; }

    bool save_profile(const PlayerProfile &profile) const override {
        stored_profile = profile;
        ++save_count;
        return true;
    }
};

class FakeProgressionCatalog final : public ProgressionCatalogPort {
  public:
    std::vector<ProgressionLevelUnlock> levels;

    [[nodiscard]] std::vector<ProgressionLevelUnlock> get_progression_level_unlocks() const override { return levels; }
};

class FakeUpgradeCatalog final : public UpgradeCatalogPort {
  public:
    std::vector<std::string> base_units;
    std::vector<ProgressionUpgradeCard> progression_cards;
    std::vector<UpgradeDraftCard> draft_cards;
    UpgradeDraftConfig draft_config{.draft_size = 1, .reserve_unit_slot = false};

    [[nodiscard]] std::vector<std::string> get_base_unit_ids() const override { return base_units; }
    [[nodiscard]] std::vector<ProgressionUpgradeCard> get_progression_upgrade_cards() const override { return progression_cards; }
    [[nodiscard]] std::vector<UpgradeDraftCard> get_upgrade_draft_cards() const override { return draft_cards; }
    [[nodiscard]] UpgradeDraftConfig get_upgrade_draft_config() const override { return draft_config; }
    [[nodiscard]] std::optional<ProgressionUpgradePresentation> find_upgrade_presentation(const std::string &upgrade_id) const override {
        return ProgressionUpgradePresentation{.id = upgrade_id, .name = upgrade_id};
    }
    [[nodiscard]] std::vector<ProgressionUpgradePresentation> get_upgrade_presentations() const override { return {}; }
};

class FakeUnitCatalog final : public UnitCatalog {
  public:
    std::vector<UnitConfig> units;

    [[nodiscard]] std::optional<UnitConfig> get_unit(const std::string &name) const override {
        const auto found = std::ranges::find(units, name, &UnitConfig::name);
        return found == units.end() ? std::nullopt : std::optional<UnitConfig>(*found);
    }

    [[nodiscard]] std::vector<UnitConfig> get_friendly_units() const override {
        std::vector<UnitConfig> result;
        std::ranges::copy_if(units, std::back_inserter(result), [](const auto &unit) { return unit.side == UnitSide::FRIENDLY; });
        return result;
    }
};

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
    profile.completed_levels.insert("level_01");
    profile.completed_levels.insert("level_02");
    const std::vector<ProgressionUpgradeCard> cards{make_card("hp", 1), make_card("speed", 2)};

    DEFN_CHECK(ProgressionUseCases::claim_level_upgrade(profile, cards, "level_01", "hp"));
    DEFN_CHECK_EQ(profile.claimed_level_upgrades["level_01"], std::string("hp"));
    DEFN_CHECK_EQ(profile.owned_upgrade_counts["hp"], 1);
    DEFN_CHECK(!ProgressionUseCases::claim_level_upgrade(profile, cards, "level_01", "speed"));
    DEFN_CHECK(!ProgressionUseCases::claim_level_upgrade(profile, cards, "level_02", "hp"));
}

DEFN_TEST(progression_use_cases_reject_level_upgrade_before_completion) {
    ProgressionProfile profile;
    const std::vector<ProgressionUpgradeCard> cards{make_card("hp", 1)};

    DEFN_CHECK(!ProgressionUseCases::claim_level_upgrade(profile, cards, "level_01", "hp"));
}

DEFN_TEST(progression_use_cases_claim_rescue_draft_for_frontier_threshold) {
    ProgressionProfile profile;
    profile.completed_levels.insert("level_01");
    profile.total_score = 250;
    const std::vector<ProgressionLevelUnlock> levels{{.level_id = "level_01"},
                                                     {.level_id = "level_02", .requires_completed = "level_01", .rescue_thresholds = {100, 250}}};
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

DEFN_TEST(progression_campaign_use_cases_load_default_profile_through_repository) {
    FakeProfileRepository profile_repository;
    FakeProgressionCatalog progression_catalog;
    FakeUpgradeCatalog upgrade_catalog;
    tests::ScriptedRandomSource random;
    ProgressionCampaignUseCases use_cases(profile_repository, progression_catalog, upgrade_catalog, random);

    const PlayerProfile profile = use_cases.load_campaign();

    DEFN_CHECK_EQ(profile.total_score, 0);
    DEFN_CHECK(profile_repository.stored_profile.has_value());
    DEFN_CHECK_EQ(profile_repository.save_count, 1);
}

DEFN_TEST(progression_campaign_use_cases_complete_level_saves_and_builds_reward) {
    FakeProfileRepository profile_repository;
    FakeProgressionCatalog progression_catalog;
    progression_catalog.levels = {{.level_id = "level_01"}, {.level_id = "level_02", .requires_completed = "level_01"}};
    FakeUpgradeCatalog upgrade_catalog;
    upgrade_catalog.progression_cards = {make_card("hp", 1)};
    upgrade_catalog.draft_cards = {{.id = "hp", .weight = 1}};
    tests::ScriptedRandomSource random;
    random.push_int(1);
    ProgressionCampaignUseCases use_cases(profile_repository, progression_catalog, upgrade_catalog, random);

    PlayerProfile profile;
    profile.total_score = 10;

    const ProgressionMatchResult result = use_cases.complete_level(profile, "level_01", 200, true);

    DEFN_CHECK_EQ(profile.total_score, 210);
    DEFN_CHECK(profile.completed_levels.contains("level_01"));
    DEFN_CHECK_EQ(result.new_total_score, 210);
    DEFN_REQUIRE(result.new_unlock_level_ids.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(result.new_unlock_level_ids[0], std::string("level_02"));
    DEFN_CHECK_EQ(result.next_level_id, std::string("level_02"));
    DEFN_CHECK_EQ(result.reward_draft.source, ProgressionRewardSource::FIRST_CLEAR);
    DEFN_REQUIRE(result.reward_draft.upgrade_ids.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(result.reward_draft.upgrade_ids[0], std::string("hp"));
    DEFN_CHECK_EQ(profile_repository.save_count, 1);
    DEFN_REQUIRE(profile_repository.stored_profile.has_value());
    DEFN_CHECK(profile_repository.stored_profile->completed_levels.contains("level_01"));
}

DEFN_TEST(progression_campaign_use_cases_claim_upgrade_saves_only_successful_claims) {
    FakeProfileRepository profile_repository;
    FakeProgressionCatalog progression_catalog;
    FakeUpgradeCatalog upgrade_catalog;
    upgrade_catalog.progression_cards = {make_card("hp", 1)};
    tests::ScriptedRandomSource random;
    ProgressionCampaignUseCases use_cases(profile_repository, progression_catalog, upgrade_catalog, random);

    PlayerProfile profile;
    DEFN_CHECK(!use_cases.claim_upgrade(profile, {.source = ProgressionRewardSource::FIRST_CLEAR, .level_id = "level_01", .upgrade_id = "hp"}));
    DEFN_CHECK_EQ(profile_repository.save_count, 0);

    profile.completed_levels.insert("level_01");
    DEFN_CHECK(use_cases.claim_upgrade(profile, {.source = ProgressionRewardSource::FIRST_CLEAR, .level_id = "level_01", .upgrade_id = "hp"}));
    DEFN_CHECK_EQ(profile.owned_upgrade_counts["hp"], 1);
    DEFN_CHECK_EQ(profile_repository.save_count, 1);
}

DEFN_TEST(progression_campaign_use_cases_build_available_roster_from_base_and_unlocks) {
    FakeProfileRepository profile_repository;
    FakeProgressionCatalog progression_catalog;
    FakeUpgradeCatalog upgrade_catalog;
    upgrade_catalog.base_units = {"guard"};
    ProgressionUpgradeCard unlock_card;
    unlock_card.id = "unlock_operator";
    unlock_card.effects.push_back({.type = ProgressionUpgradeEffectType::UNIT_UNLOCK, .unit_id = "operator"});
    upgrade_catalog.progression_cards = {unlock_card};
    tests::ScriptedRandomSource random;
    ProgressionCampaignUseCases use_cases(profile_repository, progression_catalog, upgrade_catalog, random);

    PlayerProfile profile;
    profile.owned_upgrade_counts["unlock_operator"] = 1;

    const std::vector<std::string> roster = use_cases.build_available_roster(profile);

    DEFN_REQUIRE(roster.size() == static_cast<size_t>(2));
    DEFN_CHECK_EQ(roster[0], std::string("guard"));
    DEFN_CHECK_EQ(roster[1], std::string("operator"));
}

DEFN_TEST(progression_campaign_use_cases_builds_ordered_overview_with_effective_values_and_sources) {
    FakeProfileRepository profile_repository;
    FakeProgressionCatalog progression_catalog;
    FakeUpgradeCatalog upgrade_catalog;
    upgrade_catalog.base_units = {"breacher"};
    upgrade_catalog.progression_cards = {
        {.id = "plating", .max_picks = 2, .effects = {{.type = ProgressionUpgradeEffectType::UNIT_HP_DELTA, .value = 25.0F, .unit_id = "breacher"}}},
        {.id = "unlock_marksman", .effects = {{.type = ProgressionUpgradeEffectType::UNIT_UNLOCK, .unit_id = "marksman"}}},
        {.id = "integrity", .effects = {{.type = ProgressionUpgradeEffectType::BASE_INTEGRITY_DELTA, .value = 2.0F}}},
        {.id = "energy",
         .effects = {{.type = ProgressionUpgradeEffectType::STARTING_ENERGY_DELTA, .value = 35.0F},
                     {.type = ProgressionUpgradeEffectType::ENERGY_REGEN_DELTA, .value = 1.0F},
                     {.type = ProgressionUpgradeEffectType::BOUNTY_MULTIPLIER_DELTA, .value = 0.5F}}}};
    FakeUnitCatalog units;
    units.units = {{.name = "base",
                    .description = "Friendly base.",
                    .hp = 300,
                    .ranged_damage = 12,
                    .ranged_attack_period_seconds = 1.0,
                    .animations = {{"idle", {.path_template = "res://base.png"}}}},
                   {.name = "breacher", .description = "Anchor.", .hp = 400, .ranged_damage = 8, .move_speed_pixels_per_second = 58.0F, .cost = 20},
                   {.name = "marksman", .hp = 200, .ranged_damage = 14, .move_speed_pixels_per_second = 64.0F, .cost = 30},
                   {.name = "hostile", .side = UnitSide::HOSTILE}};
    tests::ScriptedRandomSource random;
    ProgressionCampaignUseCases use_cases(profile_repository, progression_catalog, upgrade_catalog, random);
    PlayerProfile profile;
    profile.owned_upgrade_counts = {{"plating", 2}, {"integrity", 1}, {"energy", 1}};

    const auto overview = use_cases.build_progression_overview(profile, units);

    DEFN_REQUIRE(overview.entities.size() == static_cast<size_t>(4));
    DEFN_CHECK_EQ(overview.entities[0].id, std::string("base"));
    DEFN_CHECK_EQ(overview.entities[1].id, std::string("breacher"));
    DEFN_CHECK_EQ(overview.entities[2].id, std::string("marksman"));
    DEFN_CHECK_EQ(overview.entities[3].id, std::string("operations"));
    DEFN_CHECK(overview.entities[1].unlocked);
    DEFN_CHECK(!overview.entities[2].unlocked);
    DEFN_CHECK_EQ(overview.entities[1].stats[0].effective_value, 450.0);
    DEFN_REQUIRE(overview.entities[1].contributing_upgrades.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(overview.entities[1].contributing_upgrades[0].owned_count, 2);
    DEFN_CHECK(overview.entities[0].stats[3].contribution_only);
    DEFN_CHECK_EQ(overview.entities[0].stats[3].contribution, 2.0);
    DEFN_CHECK(overview.entities[3].stats[0].contribution_only);
    DEFN_CHECK_EQ(overview.entities[3].stats[0].contribution, 35.0);
    DEFN_CHECK_CLOSE(overview.entities[3].stats[2].contribution, 0.5, 0.0001);
}

} // namespace defn
