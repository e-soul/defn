#include "test_harness.h"

#include "progression_rules.h"
#include "unit_progression_mapper.h"

namespace defn {

namespace {

ProgressionUpgradeCard make_upgrade_card(const std::string &id, ProgressionUpgradeEffectType effect_type, float value, const std::string &unit_id = {}) {
    ProgressionUpgradeCard card;
    card.id = id;
    card.effects.push_back({.type = effect_type, .value = value, .unit_id = unit_id});
    return card;
}

} // namespace

DEFN_TEST(progression_rules_unlock_levels_and_find_frontier) {
    const std::vector<ProgressionLevelUnlock> unlocks{
        {.level_id = "level_01"},
        {.level_id = "level_02", .requires_completed = "level_01"},
        {.level_id = "level_03", .requires_completed = "level_02"},
    };
    ProgressionProfile profile;

    DEFN_CHECK(is_level_unlocked(profile, unlocks, "level_01"));
    DEFN_CHECK(!is_level_unlocked(profile, unlocks, "level_02"));
    DEFN_CHECK(!is_level_unlocked(profile, unlocks, "missing"));
    DEFN_CHECK_EQ(get_frontier_level_id(profile, unlocks), std::string("level_01"));

    profile.completed_levels.insert("level_01");

    DEFN_CHECK(is_level_unlocked(profile, unlocks, "level_02"));
    DEFN_CHECK_EQ(get_frontier_level_id(profile, unlocks), std::string("level_02"));
}

DEFN_TEST(progression_rules_allow_level_upgrade_only_after_completion) {
    ProgressionProfile profile;

    DEFN_CHECK(!can_claim_level_upgrade(profile, "level_01"));

    profile.completed_levels.insert("level_01");
    DEFN_CHECK(can_claim_level_upgrade(profile, "level_01"));

    profile.claimed_level_upgrades["level_01"] = "hp";
    DEFN_CHECK(!can_claim_level_upgrade(profile, "level_01"));
}

DEFN_TEST(progression_rules_rescue_drafts_follow_frontier_thresholds) {
    const std::vector<ProgressionLevelUnlock> unlocks{
        {.level_id = "level_01"},
        {.level_id = "level_02", .requires_completed = "level_01", .rescue_thresholds = {100, 250}},
    };
    ProgressionProfile profile;
    profile.completed_levels.insert("level_01");
    profile.total_score = 99;

    DEFN_CHECK_EQ(get_next_rescue_draft_threshold(profile, unlocks, "level_02"), 100);
    DEFN_CHECK(!can_claim_rescue_draft(profile, unlocks, "level_02"));

    profile.total_score = 100;
    DEFN_CHECK(can_claim_rescue_draft(profile, unlocks, "level_02"));

    profile.claimed_rescue_drafts["level_02"] = 1;
    DEFN_CHECK_EQ(get_next_rescue_draft_threshold(profile, unlocks, "level_02"), 250);
    profile.total_score = 249;
    DEFN_CHECK(!can_claim_rescue_draft(profile, unlocks, "level_02"));
    profile.total_score = 250;
    DEFN_CHECK(can_claim_rescue_draft(profile, unlocks, "level_02"));

    profile.completed_levels.insert("level_02");
    DEFN_CHECK(!can_claim_rescue_draft(profile, unlocks, "level_02"));
}

DEFN_TEST(progression_rules_apply_owned_unit_upgrade_effects) {
    ProgressionProfile profile;
    profile.owned_upgrade_counts["hp"] = 2;
    profile.owned_upgrade_counts["ranged"] = 1;
    profile.owned_upgrade_counts["speed"] = 1;

    const std::vector<ProgressionUpgradeCard> cards{
        make_upgrade_card("hp", ProgressionUpgradeEffectType::UNIT_HP_DELTA, 25.0F, "guard"),
        make_upgrade_card("ranged", ProgressionUpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA, 2.4F, "guard"),
        make_upgrade_card("speed", ProgressionUpgradeEffectType::UNIT_MOVE_SPEED_DELTA, 5.0F, "guard"),
    };

    ProgressionUnitStats base;
    base.unit_id = "guard";
    base.friendly = true;
    base.hp = 10;
    base.ranged_damage = 0;
    base.move_speed = 0.0F;

    const ProgressionUnitStats effective = apply_owned_upgrade_effects(profile, cards, base);

    DEFN_CHECK_EQ(effective.hp, 60);
    DEFN_CHECK_EQ(effective.ranged_damage, 2);
    DEFN_CHECK_CLOSE(effective.move_speed, 5.0, 0.001);
}

DEFN_TEST(progression_rules_preserve_passive_stationary_unit_caps) {
    ProgressionProfile profile;
    profile.owned_upgrade_counts["hp"] = 1;

    const std::vector<ProgressionUpgradeCard> cards{make_upgrade_card("hp", ProgressionUpgradeEffectType::UNIT_HP_DELTA, 5.0F, "tower")};

    ProgressionUnitStats base;
    base.unit_id = "tower";
    base.friendly = true;
    base.hp = 20;
    base.ranged_damage = 0;
    base.move_speed = 0.0F;
    base.has_projectile_attack = false;

    const ProgressionUnitStats effective = apply_owned_upgrade_effects(profile, cards, base);

    DEFN_CHECK_EQ(effective.hp, 25);
    DEFN_CHECK_EQ(effective.ranged_damage, 0);
    DEFN_CHECK_CLOSE(effective.move_speed, 0.0, 0.001);
}

DEFN_TEST(progression_rules_calculate_campaign_modifiers) {
    ProgressionProfile profile;
    profile.owned_upgrade_counts["energy"] = 2;
    profile.owned_upgrade_counts["regen"] = 1;
    profile.owned_upgrade_counts["bounty"] = 1;
    profile.owned_upgrade_counts["integrity"] = 1;

    const std::vector<ProgressionUpgradeCard> cards{
        make_upgrade_card("energy", ProgressionUpgradeEffectType::STARTING_ENERGY_DELTA, 10.0F),
        make_upgrade_card("regen", ProgressionUpgradeEffectType::ENERGY_REGEN_DELTA, 1.0F),
        make_upgrade_card("bounty", ProgressionUpgradeEffectType::BOUNTY_MULTIPLIER_DELTA, 0.25F),
        make_upgrade_card("integrity", ProgressionUpgradeEffectType::BASE_INTEGRITY_DELTA, 2.0F),
    };

    const CampaignModifiers modifiers = calculate_campaign_modifiers(profile, cards);

    DEFN_CHECK_EQ(modifiers.starting_energy_delta, 20);
    DEFN_CHECK_EQ(modifiers.energy_regen, 2);
    DEFN_CHECK_CLOSE(modifiers.bounty_multiplier, 1.25, 0.001);
    DEFN_CHECK_EQ(modifiers.base_integrity_delta, 2);
}

DEFN_TEST(unit_progression_mapper_converts_all_progression_fields) {
    UnitConfig config;
    config.name = "raider";
    config.side = UnitSide::HOSTILE;
    config.hp = 175;
    config.ranged_damage = 23;
    config.move_speed_pixels_per_second = 81.5F;
    config.projectile_attack = ProjectileAttackConfig{};

    const ProgressionUnitStats stats = to_progression_unit_stats(config);

    DEFN_CHECK_EQ(stats.unit_id, std::string("raider"));
    DEFN_CHECK(!stats.friendly);
    DEFN_CHECK_EQ(stats.hp, 175);
    DEFN_CHECK_EQ(stats.ranged_damage, 23);
    DEFN_CHECK_CLOSE(stats.move_speed, 81.5, 0.001);
    DEFN_CHECK(stats.has_projectile_attack);
}

DEFN_TEST(unit_progression_mapper_applies_stats_to_a_copy_only) {
    UnitConfig base;
    base.name = "operator";
    base.hp = 100;
    base.ranged_damage = 8;
    base.move_speed_pixels_per_second = 64.0F;
    base.cost = 25;
    base.melee_damage = 17;

    const UnitConfig effective = with_progression_unit_stats(
        base, {.unit_id = "operator", .friendly = true, .hp = 140, .ranged_damage = 13, .move_speed = 72.5F, .has_projectile_attack = false});

    DEFN_CHECK_EQ(effective.hp, 140);
    DEFN_CHECK_EQ(effective.ranged_damage, 13);
    DEFN_CHECK_CLOSE(effective.move_speed_pixels_per_second, 72.5, 0.001);
    DEFN_CHECK_EQ(effective.name, base.name);
    DEFN_CHECK_EQ(effective.cost, base.cost);
    DEFN_CHECK_EQ(effective.melee_damage, base.melee_damage);
    DEFN_CHECK_EQ(base.hp, 100);
    DEFN_CHECK_EQ(base.ranged_damage, 8);
    DEFN_CHECK_CLOSE(base.move_speed_pixels_per_second, 64.0, 0.001);
}

} // namespace defn
