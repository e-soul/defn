#include "test_harness.h"

#include "progression_stat_visualization.h"

#include <array>
#include <cstddef>

namespace defn {

namespace {

void check_fractions(const std::array<ProgressionStatSegmentViewModel, PROGRESSION_STAT_SEGMENT_COUNT> &segments,
                     const std::array<double, PROGRESSION_STAT_SEGMENT_COUNT> &expected) {
    for (std::size_t index = 0; index < segments.size(); ++index) {
        DEFN_CHECK_CLOSE(segments[index].promotion_fraction, expected[index], 0.000001);
    }
}

} // namespace

DEFN_TEST(progression_stat_visualization_normalizes_empty_partial_and_tier_one_health) {
    check_fractions(normalize_progression_stat(-5.0, 100.0), {0.0, 0.0, 0.0, 0.0, 0.0});
    check_fractions(normalize_progression_stat(400.0, 100.0), {1.0, 1.0, 1.0, 1.0, 0.0});
    check_fractions(normalize_progression_stat(450.0, 100.0), {1.0, 1.0, 1.0, 1.0, 0.5});
    const auto boundary = normalize_progression_stat(500.0, 100.0);
    check_fractions(boundary, {1.0, 1.0, 1.0, 1.0, 1.0});
    DEFN_CHECK_EQ(boundary.front().foundation_tier, -1);
    DEFN_CHECK_EQ(boundary.front().promoted_tier, 0);
}

DEFN_TEST(progression_stat_visualization_promotes_layered_overflow_tiers) {
    const auto six_hundred = normalize_progression_stat(600.0, 100.0);
    check_fractions(six_hundred, {1.0, 0.0, 0.0, 0.0, 0.0});
    DEFN_CHECK_EQ(six_hundred.front().foundation_tier, 0);
    DEFN_CHECK_EQ(six_hundred.front().promoted_tier, 1);

    const auto one_thousand = normalize_progression_stat(1000.0, 100.0);
    check_fractions(one_thousand, {1.0, 1.0, 1.0, 1.0, 1.0});
    DEFN_CHECK_EQ(one_thousand.front().promoted_tier, 1);

    const auto eleven_hundred = normalize_progression_stat(1100.0, 100.0);
    check_fractions(eleven_hundred, {0.5, 0.0, 0.0, 0.0, 0.0});
    DEFN_CHECK_EQ(eleven_hundred.front().foundation_tier, 1);
    DEFN_CHECK_EQ(eleven_hundred.front().promoted_tier, 2);

    check_fractions(normalize_progression_stat(1200.0, 100.0), {1.0, 0.0, 0.0, 0.0, 0.0});
    DEFN_CHECK_EQ(normalize_progression_stat(80000.0, 100.0).front().promoted_tier, 8);
}

DEFN_TEST(progression_stat_visualization_handles_fractional_quanta_and_upgrade_emphasis) {
    check_fractions(normalize_progression_stat(0.3, 0.25), {1.0, 0.2, 0.0, 0.0, 0.0});
    check_fractions(normalize_progression_stat(1.25, 0.25), {1.0, 1.0, 1.0, 1.0, 1.0});

    const auto visual = make_progression_stat_visual("health", 400.0, 450.0, "450 HP", false);
    for (std::size_t index = 0; index < visual.segments.size() - 1; ++index) {
        DEFN_CHECK(!visual.segments[index].upgrade_emphasis);
    }
    DEFN_CHECK(visual.segments.back().upgrade_emphasis);
}

DEFN_TEST(progression_stat_visualization_configures_known_contribution_and_unknown_stats) {
    DEFN_CHECK_CLOSE(progression_stat_config("health").quantum, 100.0, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("health").icon, ProgressionStatIcon::SHIELD);
    DEFN_CHECK_CLOSE(progression_stat_config("firepower").quantum, 4.0, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("firepower").icon, ProgressionStatIcon::RETICLE);
    DEFN_CHECK_CLOSE(progression_stat_config("mobility").quantum, 20.0, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("mobility").icon, ProgressionStatIcon::MOBILITY);
    DEFN_CHECK_CLOSE(progression_stat_config("deploy_cost").quantum, 5.0, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("deploy_cost").direction, ProgressionStatDirection::MORE_IS_EXPENSIVE);
    DEFN_CHECK_CLOSE(progression_stat_config("fire_rate").quantum, 0.25, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("fire_rate").icon, ProgressionStatIcon::FIRE_RATE);
    DEFN_CHECK_CLOSE(progression_stat_config("integrity_bonus").quantum, 1.0, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("integrity_bonus").icon, ProgressionStatIcon::INTEGRITY);
    DEFN_CHECK_CLOSE(progression_stat_config("starting_energy_bonus").quantum, 10.0, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("starting_energy_bonus").icon, ProgressionStatIcon::ENERGY);
    DEFN_CHECK_CLOSE(progression_stat_config("energy_generation").quantum, 0.5, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("energy_generation").icon, ProgressionStatIcon::ENERGY);
    DEFN_CHECK_CLOSE(progression_stat_config("bounty_bonus").quantum, 10.0, 0.000001);
    DEFN_CHECK_EQ(progression_stat_config("bounty_bonus").icon, ProgressionStatIcon::BOUNTY);
    DEFN_CHECK_EQ(progression_stat_config("missing").icon, ProgressionStatIcon::GENERIC);
    DEFN_CHECK_CLOSE(progression_stat_config("missing").quantum, 1.0, 0.000001);

    const auto bounty = make_progression_stat_visual("bounty_bonus", 0.0, 0.5, "+50%", true);
    check_fractions(bounty.segments, {1.0, 1.0, 1.0, 1.0, 1.0});
    DEFN_CHECK(bounty.segments.front().upgrade_emphasis);
}

} // namespace defn
